#include "engine.h"
#include "fsm.h"
#include "fsm_gameplay.h"
#include "fsm_events.h"
#include "fsm_states.h"
#include "gamestate.h"
#include "game_config.h"
#include "button_hal.h"
#include "defense_matrix.h"
#include "threat.h"
#include "entity_pool.h"
#include "y_sort.h"
#include "screen_tarefa_amarela.h"
#include "screen_web_setor.h"
#include "ui.h"

#include "lvgl.h"
#include "esp_log.h"
#include "esp_random.h"

#include <string.h>
#include <stdbool.h>

static const char *TAG = "SIM_ENGINE";

static bool             s_initialized = false;
static lv_timer_t      *s_tick_timer  = NULL;

static button_state_t   s_prev_btn[BTN_MAX_COUNT];
static game_state_t     s_last_macro  = GAME_STATE_SPLASH;
static gameplay_sala_t  s_last_sala   = GAMEPLAY_SALA_RECEPCAO;

static bool          s_sim_server_lost[THREAT_SERVER_COUNT];
static ataque_tipo_t s_sim_server_lost_tipo[THREAT_SERVER_COUNT];

/* === Card resolver (mesmo logic do engine.c) ============================ */

static int sim_card_resolver(int mock_card)
{
    uint8_t srv = 0;
    threat_state_t st;
    if (!threat_get_active(0, &st)) {
        if (!threat_get_active(1, &st)) return -1;
        srv = 1;
    }
    carta_id_t carta;
    if (mock_card == 0) {
        /* Ransomware congelado: a carta "correta" passa a ser o Backup. */
        carta = threat_is_congelado(srv) ? CARTA_BACKUP
                                         : threat_carta_correta(st.tipo);
    } else {
        const carta_id_t certa = threat_carta_correta(st.tipo);
        carta = (certa == CARTA_ISOLAMENTO) ? CARTA_BACKUP : CARTA_ISOLAMENTO;
    }
    const defesa_resultado_t r = threat_mitigate(srv, carta);
    if (r == DEFESA_CORRETO || r == DEFESA_CONTIDO) return 0;
    return (r == DEFESA_INUTIL) ? 1 : 2;
}

/* === UI sync ============================================================= */

static void sync_gameplay_sala_to_ui(gameplay_sala_t sala)
{
    switch (sala) {
        case GAMEPLAY_SALA_RECEPCAO: ui_show_recepcao(); break;
        case GAMEPLAY_SALA_EMPRESA:  ui_show_empresa();  break;
        default: break;
    }
}

static void sync_ui_to_macro(game_state_t macro)
{
    switch (macro) {
        case GAME_STATE_SPLASH:    ui_show_splash();    break;
        case GAME_STATE_MENU:      ui_show_menu();      break;
        case GAME_STATE_GAMEPLAY:  sync_gameplay_sala_to_ui(fsm_get_gameplay_sala()); break;
        case GAME_STATE_PAUSE:     ui_show_pause();     break;
        case GAME_STATE_GAME_OVER: ui_show_game_over(); break;
        default: break;
    }
}

/* === Model tick (relogio + ataques + vitoria/derrota) ==================== */

static void gameplay_model_tick(uint32_t dt_ms)
{
    if (fsm_get_state() != GAME_STATE_GAMEPLAY) return;

    gamestate_tick(dt_ms);

    {
        threat_state_t ts;
        fsm_set_attack_active(threat_get_active(0, &ts) ||
                              threat_get_active(1, &ts));
    }

    /* Sincroniza ataques com a tela do setor web — por servidor. */
    {
        for (uint8_t srv = 0; srv < THREAT_SERVER_COUNT; srv++) {
            threat_state_t ts_ws;
            if (threat_get_active(srv, &ts_ws)) {
                const web_setor_id_t wsid = (web_setor_id_t)srv;
                if (ts_ws.tipo == ATAQUE_DDOS) {
                    if (screen_web_setor_is_open(wsid))
                        screen_web_setor_ddos_start(wsid);
                } else if (ts_ws.tipo == ATAQUE_RANSOMWARE) {
                    if (screen_web_setor_is_open(wsid))
                        screen_web_setor_ransomware_start(wsid);
                }
            }
        }
    }

    /* threat_tick cuida do gating via gamestate — sempre chamar. */
    for (uint8_t srv = 0; srv < THREAT_SERVER_COUNT; srv++) {
        threat_state_t ts_pre;
        const bool pre_active = threat_get_active(srv, &ts_pre);

        if (threat_tick(srv, dt_ms)) {
            if (pre_active) {
                s_sim_server_lost[srv]      = true;
                s_sim_server_lost_tipo[srv] = ts_pre.tipo;
            }
            gamestate_perder_vida();
            ESP_LOGW(TAG, "srv%u destruido! vidas=%u",
                     (unsigned)srv, gamestate_get_vidas());
            if (gamestate_get_vidas() == 0) {
                gamestate_set_result(RESULT_DERROTA);
                fsm_set_state(GAME_STATE_GAME_OVER);
                return;
            }
        }
    }

    if (gamestate_get_clock_minutes() >= HORA_FIM_JOGO_MIN) {
        gamestate_set_result(RESULT_VITORIA);
        ESP_LOGI(TAG, "18:00 — expediente concluido -> VITORIA");
        fsm_set_state(GAME_STATE_GAME_OVER);
    }
}

/* === Timer callback principal =========================================== */

static void sim_engine_tick(lv_timer_t *t)
{
    (void)t;
    const uint32_t dt_ms = ENGINE_TICK_PERIOD_MS;

    /* Detecta edges de botao -> eventos FSM */
    for (int i = 0; i < BTN_MAX_COUNT; i++) {
        const button_state_t now = button_hal_peek((button_id_t)i);
        if (now != s_prev_btn[i]) {
            s_prev_btn[i] = now;
            const fsm_event_t ev = {
                .kind = FSM_EVT_BUTTON,
                .payload.button.id    = (uint8_t)i,
                .payload.button.state = (uint8_t)now,
            };
            fsm_handle_event(&ev);
        }
    }

    /* Tick da FSM */
    const fsm_event_t tick = { .kind = FSM_EVT_TICK, .payload.tick.dt_ms = dt_ms };
    fsm_handle_event(&tick);

    /* Tick do modelo de jogo */
    gameplay_model_tick(dt_ms);

    /* Sincroniza render (Y-sort + debug overlay) */
    entity_render_sync();

    /* Detecta mudancas de tela */
    const game_state_t    cur_macro = fsm_get_state();
    const gameplay_sala_t cur_sala  = fsm_get_gameplay_sala();

    if (cur_macro != s_last_macro) {
        if (cur_macro == GAME_STATE_GAMEPLAY && s_last_macro != GAME_STATE_PAUSE) {
            /* Nova run: zera relogio + ataques */
            gamestate_reset();
            threat_init();
            screen_tarefa_amarela_reset();
            fsm_set_attack_active(false);
            memset(s_sim_server_lost,      0, sizeof(s_sim_server_lost));
            memset(s_sim_server_lost_tipo, 0, sizeof(s_sim_server_lost_tipo));
            ESP_LOGI(TAG, "nova run: relogio + ataques zerados");
        }
        sync_ui_to_macro(cur_macro);
        s_last_macro = cur_macro;
    } else if (cur_macro == GAME_STATE_GAMEPLAY && cur_sala != s_last_sala) {
        sync_gameplay_sala_to_ui(cur_sala);
    }
    s_last_sala = cur_sala;
}

/* === API publica ========================================================= */

esp_err_t engine_init(void)
{
    if (s_initialized) return ESP_OK;

    fsm_init();
    entity_pool_init();
    y_sort_init();
    gamestate_init();
    threat_init();
    fsm_set_card_resolver(sim_card_resolver);

    memset(s_prev_btn, 0, sizeof(s_prev_btn));
    s_last_macro = GAME_STATE_SPLASH;
    s_last_sala  = GAMEPLAY_SALA_RECEPCAO;

    ui_init();
    ui_show_splash();

    s_initialized = true;
    ESP_LOGI(TAG, "engine_init OK");
    return ESP_OK;
}

esp_err_t engine_start(void)
{
    if (!s_initialized || s_tick_timer != NULL) return ESP_OK;
    s_tick_timer = lv_timer_create(sim_engine_tick, ENGINE_TICK_PERIOD_MS, NULL);
    ESP_LOGI(TAG, "engine_start OK (tick=%dms)", ENGINE_TICK_PERIOD_MS);
    return ESP_OK;
}

void engine_set_test_mode(bool enable) { (void)enable; }

bool engine_server_is_lost(uint8_t srv)
{
    return srv < THREAT_SERVER_COUNT && s_sim_server_lost[srv];
}

const char *engine_server_lost_nome(uint8_t srv)
{
    if (!engine_server_is_lost(srv)) return "";
    return ataque_nome(s_sim_server_lost_tipo[srv]);
}
