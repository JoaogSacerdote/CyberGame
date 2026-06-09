#include "engine.h"
#include "game_config.h"
#include "calibracao.h"

/* Escala um valor de brilho LED (0-255) pelo CAL_BRILHO_LED (0-100). */
#define LED_SCALE(v)  ((uint8_t)((unsigned)(v) * (unsigned)CAL_BRILHO_LED / 100u))

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_check.h"
#include "esp_log.h"
#include "esp_task_wdt.h"

#include "fsm.h"
#include "fsm_gameplay.h"
#include "ui.h"
#include "screen_tarefa_amarela.h"
#include "button_hal.h"
#include "joystick_hal.h"
#include "ws2812_hal.h"

#include "debug_overlay.h"
#include "entity_pool.h"
#include "y_sort.h"
#include "defense_matrix.h"
#include "threat.h"
#include "gamestate.h"
#include "game_config.h"
#include "esp_random.h"
#include "nfc_hal.h"
#include "nfc_config.h"
#include "screen_tarefa_vermelha.h"
#include <string.h>

/* Combo de toggle do debug overlay: X+Y segurados continuamente por
 * DEBUG_COMBO_HOLD_MS. Botoes opostos, raramente segurados juntos
 * em gameplay normal. Y+START ja eh usado em main.c para detect_dev_combo. */
#define DEBUG_COMBO_HOLD_MS   2000

/* === LED Animation ======================================================= */
typedef enum { LED_ANIM_NONE = 0, LED_ANIM_DEFEAT, LED_ANIM_MITIG } led_anim_t;

static led_anim_t s_led_anim    = LED_ANIM_NONE;
static uint8_t    s_led_step    = 0;     /* 0-5: 3 blinks; step%2==0 → ON */
static uint32_t   s_led_step_ms = 0;
static bool       s_req_defeat  = false; /* sinalizado por gameplay_model_tick */
static bool       s_req_mitig   = false; /* sinalizado por engine_card_resolver */
static uint32_t   s_chaos_ms    = 0;     /* timer para fase caotica (85%+) */

#define LED_BLINK_STEP_MS    180   /* meio-ciclo ON ou OFF (3 blinks = 1.08 s) */
#define LED_CHAOS_PERIOD_MS  110   /* taxa de atualizacao caotica (~9 Hz) */

static const char *TAG = "ENGINE";

static bool s_nfc_initialized = false;

static carta_id_t nfc_resolve_uid(const nfc_card_t *card)
{
    if (card->uid_len >= 4) {
        ESP_LOGI(TAG, "NFC UID: %02X:%02X:%02X:%02X (len=%u) — copie para nfc_config.h",
                 card->uid[0], card->uid[1], card->uid[2], card->uid[3], card->uid_len);
    }
    for (int i = 0; i < (int)CARTA_MAX_COUNT; i++) {
        if (card->uid_len == NFC_CARTAS[i].uid_len &&
            memcmp(card->uid, NFC_CARTAS[i].uid, card->uid_len) == 0) {
            ESP_LOGI(TAG, "NFC: carta reconhecida: %s", NFC_CARTAS[i].nome);
            return NFC_CARTAS[i].carta;
        }
    }
    ESP_LOGW(TAG, "NFC: UID desconhecido — assumindo CARTA_BALANCEAMENTO (atualize nfc_config.h)");
    return CARTA_BALANCEAMENTO;
}

static void nfc_poll_tick(void)
{
    if (!s_nfc_initialized) return;
    if (fsm_get_state() != GAME_STATE_GAMEPLAY) return;
    if (fsm_get_gameplay_sala() != GAMEPLAY_SALA_EMPRESA) return;
    if (!screen_tarefa_vermelha_is_open()) return;

    nfc_card_t card;
    if (!nfc_hal_wait_card(&card, 0)) return;

    const carta_id_t carta = nfc_resolve_uid(&card);
    const defesa_resultado_t r = threat_mitigate(carta);
    if (r == DEFESA_CORRETO) {
        s_req_mitig = true;
        ESP_LOGI(TAG, "NFC: DDoS mitigado com carta %d", (int)carta);
    } else if (r == DEFESA_AGRAVA) {
        ESP_LOGW(TAG, "NFC: carta agravou o ataque");
    } else {
        ESP_LOGI(TAG, "NFC: sem ataque ativo ou carta inutil (res=%d)", (int)r);
    }
}

#define ENGINE_QUEUE_DEPTH    32
#define ENGINE_TASK_STACK     4096
#define ENGINE_TASK_PRIO      6
#define ENGINE_TASK_CORE      0           /* Core 0; LVGL fica em Core 1 */

static QueueHandle_t s_event_queue   = NULL;
static TaskHandle_t  s_task          = NULL;
static bool          s_initialized   = false;
static bool          s_test_mode     = false;   /* jogador-fantasma (DEV) */

void engine_set_test_mode(bool enable) { s_test_mode = enable; }

static void button_reader_task(void *pv)
{
    (void)pv;
    button_event_t bev;
    while (1) {
        if (button_hal_get_event(&bev, UINT32_MAX)) {
            const fsm_event_t fev = {
                .kind = FSM_EVT_BUTTON,
                .payload.button.id    = (uint8_t)bev.id,
                .payload.button.state = (uint8_t)bev.state,
            };
            xQueueSend(s_event_queue, &fev, 0);
        }
    }
}

static void sync_gameplay_sala_to_ui(gameplay_sala_t sala)
{
    switch (sala) {
        case GAMEPLAY_SALA_RECEPCAO: ui_show_recepcao(); break;
        case GAMEPLAY_SALA_EMPRESA:  ui_show_empresa();  break;
        default: break;
    }
}

/* Antes de qualquer troca de screen LVGL, libera o debug overlay para
 * nao deixar lv_obj orfao apontando para a screen morta. set_enabled(true)
 * subsequente re-cria automaticamente na nova screen. */
static void release_debug_overlay_for_screen_change(void)
{
    /* lv_lock interno — ui_router faz o mesmo padrao por dentro. */
    extern void lv_lock(void);
    extern void lv_unlock(void);
    lv_lock();
    debug_overlay_deinit();
    lv_unlock();
}

/* Sincroniza a tela ativa com o estado macro da FSM. Em GAMEPLAY, escolhe
 * a tela com base na sala atual (RECEPCAO ou EMPRESA). */
static void sync_ui_to_macro(game_state_t macro)
{
    release_debug_overlay_for_screen_change();
    switch (macro) {
        case GAME_STATE_SPLASH:   ui_show_splash();  break;
        case GAME_STATE_MENU:     ui_show_menu();    break;
        case GAME_STATE_GAMEPLAY: sync_gameplay_sala_to_ui(fsm_get_gameplay_sala()); break;
        case GAME_STATE_PAUSE:     ui_show_pause();     break;
        case GAME_STATE_GAME_OVER: ui_show_game_over(); break;
        default:                   /* sem tela ainda */ break;
    }
}

/* Polling do combo X+Y para toggle do debug overlay. Chamado uma vez
 * por tick do engine_task (a cada ENGINE_TICK_PERIOD_MS). */
static void update_debug_combo(uint32_t dt_ms)
{
    static uint32_t held_ms = 0;
    static bool     fired   = false;

    const bool both_down =
        (button_hal_peek(BTN_X) == BTN_PRESSED) &&
        (button_hal_peek(BTN_Y) == BTN_PRESSED);

    if (!both_down) {
        held_ms = 0;
        fired   = false;
        return;
    }

    held_ms += dt_ms;
    if (held_ms >= DEBUG_COMBO_HOLD_MS && !fired) {
        fired = true;        /* dispara uma unica vez por toque continuo */
        extern void lv_lock(void);
        extern void lv_unlock(void);
        lv_lock();
        const bool new_state = !debug_overlay_is_enabled();
        debug_overlay_set_enabled(new_state);
        lv_unlock();
        ESP_LOGI(TAG, "combo X+Y: debug_overlay -> %s", new_state ? "ON" : "OFF");
    }
}

/* === Loop de jogo (ataques + vitoria/derrota) =========================== */

/* Resolver de carta registrado na FSM. mock_card: 0 = carta correta pro
 * ataque ativo, 1 = carta errada (placeholder ate a leitura NFC real).
 * Retorna 0=CORRETO, 1=INUTIL, 2=AGRAVA, -1=sem ataque ativo. */
static int engine_card_resolver(int mock_card)
{
    threat_state_t st;
    if (!threat_get_active(&st)) {
        return -1;
    }
    carta_id_t carta;
    if (mock_card == 0) {
        carta = threat_carta_correta(st.tipo);
    } else {
        const carta_id_t certa = threat_carta_correta(st.tipo);
        carta = (certa == CARTA_ISOLAMENTO) ? CARTA_BACKUP : CARTA_ISOLAMENTO;
    }
    const defesa_resultado_t r = threat_mitigate(carta);
    if (r == DEFESA_CORRETO) s_req_mitig = true;
    return (r == DEFESA_CORRETO) ? 0 : (r == DEFESA_INUTIL ? 1 : 2);
}

/* LEDs durante GAMEPLAY (empresa):
 *
 *  Sem ataque:   LED0=verde (tarefa verde)  LED1=amarelo (tarefa amarela)  LED2=apagado
 *
 *  Ataque ativo (progress %):
 *    0–29 %  : LED2 vermelho; LED0+LED1 normais
 *   30–59 %  : LED1+LED2 vermelho; LED0 normal
 *   60–84 %  : todos 3 vermelho solido
 *   85–99 %  : todos 3 piscam individualmente de forma caotica/aleatoria
 *
 *  Animacao de derrota  (ataque expirou):  3x piscam vermelho juntos (1.08s)
 *  Animacao de mitigacao (carta correta):  3x piscam verde juntos    (1.08s)
 */
static void gameplay_leds_tick(uint32_t dt_ms)
{
    if (fsm_get_state() != GAME_STATE_GAMEPLAY) return;

    if (fsm_get_gameplay_sala() != GAMEPLAY_SALA_EMPRESA) {
        ws2812_hal_clear();
        ws2812_hal_refresh();
        s_led_anim   = LED_ANIM_NONE;
        s_req_defeat = false;
        s_req_mitig  = false;
        s_chaos_ms   = 0;
        return;
    }

    /* Inicia animacao se solicitada e nenhuma estiver rodando.
     * Derrota tem prioridade sobre mitigacao se ambas chegarem juntas. */
    if (s_led_anim == LED_ANIM_NONE) {
        if (s_req_defeat) {
            s_led_anim = LED_ANIM_DEFEAT;
            s_led_step = 0; s_led_step_ms = 0;
            s_req_defeat = false; s_req_mitig = false;
        } else if (s_req_mitig) {
            s_led_anim = LED_ANIM_MITIG;
            s_led_step = 0; s_led_step_ms = 0;
            s_req_mitig = false;
        }
    }

    /* Avanca e exibe animacao ativa */
    if (s_led_anim != LED_ANIM_NONE) {
        s_led_step_ms += dt_ms;
        if (s_led_step_ms >= LED_BLINK_STEP_MS) {
            s_led_step_ms -= LED_BLINK_STEP_MS;
            s_led_step++;
            if (s_led_step >= 6) {   /* 3 blinks completos */
                s_led_anim = LED_ANIM_NONE;
            }
        }
        if (s_led_anim != LED_ANIM_NONE) {
            const bool on = ((s_led_step & 1u) == 0);
            ws2812_hal_clear();
            if (on) {
                const uint8_t r = (s_led_anim == LED_ANIM_DEFEAT) ? LED_SCALE(120) : 0;
                const uint8_t g = (s_led_anim == LED_ANIM_MITIG)  ? LED_SCALE(80)  : 0;
                ws2812_hal_set_pixel(0, r, g, 0);
                ws2812_hal_set_pixel(1, r, g, 0);
                ws2812_hal_set_pixel(2, r, g, 0);
            }
            ws2812_hal_refresh();
            return;
        }
        /* Animacao terminou: cai para comportamento normal abaixo */
    }

    /* Comportamento normal baseado no progresso do ataque */
    threat_state_t ts;
    const bool atk = threat_get_active(&ts);
    const uint8_t pct = atk ? threat_progress_pct() : 0;

    if (!atk) {
        /* LED acende apenas se a tarefa esta DISPONIVEL (nao concluida, nao pendente). */
        const bool vd_on = (gamestate_verde_estado()   == TAREFA_DISPONIVEL);
        const bool am_on = (gamestate_amarela_estado()  == TAREFA_DISPONIVEL);
        ws2812_hal_set_pixel(0, 0, vd_on ? LED_SCALE(80) : 0, 0);
        ws2812_hal_set_pixel(1, am_on ? LED_SCALE(100) : 0, am_on ? LED_SCALE(100) : 0, 0);
        ws2812_hal_set_pixel(2, 0, 0, 0);
        ws2812_hal_refresh();
        return;
    }

    /* 85–99%: caos — cada LED pisca individualmente de forma aleatoria */
    if (pct >= 85) {
        s_chaos_ms += dt_ms;
        if (s_chaos_ms >= LED_CHAOS_PERIOD_MS) {
            s_chaos_ms -= LED_CHAOS_PERIOD_MS;
            const uint32_t rnd = esp_random();
            ws2812_hal_clear();
            if (rnd & 1u) ws2812_hal_set_pixel(0, LED_SCALE(120), 0, 0);
            if (rnd & 2u) ws2812_hal_set_pixel(1, LED_SCALE(120), 0, 0);
            if (rnd & 4u) ws2812_hal_set_pixel(2, LED_SCALE(120), 0, 0);
            ws2812_hal_refresh();
        }
        return;
    }

    ws2812_hal_clear();
    if (pct >= 60) {
        /* 60–84%: todos 3 vermelho solido */
        ws2812_hal_set_pixel(0, LED_SCALE(120), 0, 0);
        ws2812_hal_set_pixel(1, LED_SCALE(120), 0, 0);
        ws2812_hal_set_pixel(2, LED_SCALE(120), 0, 0);
    } else if (pct >= 30) {
        /* 30–59%: LED1+LED2 vermelho; LED0 normal */
        ws2812_hal_set_pixel(0, 0,              LED_SCALE(80),  0);
        ws2812_hal_set_pixel(1, LED_SCALE(120), 0,              0);
        ws2812_hal_set_pixel(2, LED_SCALE(120), 0,              0);
    } else {
        /* 0–29%: so LED2 vermelho; LED0+LED1 normais */
        ws2812_hal_set_pixel(0, 0,              LED_SCALE(80),  0);
        ws2812_hal_set_pixel(1, LED_SCALE(100), LED_SCALE(100), 0);
        ws2812_hal_set_pixel(2, LED_SCALE(120), 0,              0);
    }
    ws2812_hal_refresh();
}

/* Tick do modelo durante GAMEPLAY: relogio + ataques + vitoria/derrota. */
static void gameplay_model_tick(uint32_t dt_ms)
{
    if (fsm_get_state() != GAME_STATE_GAMEPLAY) {
        return;
    }
    gamestate_tick(dt_ms);

    /* Mantém flag de ataque na FSM para que as telas UI possam ler
     * sem dependência circular em threat.h. */
    {
        threat_state_t ts_flag;
        fsm_set_attack_active(threat_get_active(&ts_flag));
    }

    /* Ataques vermelhos so iniciam apos TAREFA_VERMELHO_MIN_MS de expediente. */
    if (gamestate_vermelho_pode_spawnar()) {
        if (threat_tick(dt_ms)) {        /* ataque expirou -> setor destruido */
            s_req_defeat = true;
            gamestate_perder_vida();
            ESP_LOGW(TAG, "[LOOP] setor destruido! vidas=%u", gamestate_get_vidas());
            if (gamestate_get_vidas() == 0) {
                gamestate_set_result(RESULT_DERROTA);
                ESP_LOGW(TAG, "[LOOP] sem vidas -> DERROTA");
                fsm_set_state(GAME_STATE_GAME_OVER);
                return;
            }
        }
    }

    if (gamestate_get_clock_minutes() >= HORA_FIM_JOGO_MIN) {
        gamestate_set_result(RESULT_VITORIA);
        ESP_LOGI(TAG, "[LOOP] 18:00 — expediente concluido -> VITORIA");
        fsm_set_state(GAME_STATE_GAME_OVER);
    }
}

/* Simulacao do loop no boot — validacao REMOTA (sem precisar jogar). Roda
 * dois cenarios rapidos e loga o desfecho. Reseta no fim (nao afeta a run
 * real). Aux temporario. */
static void gameplay_sim_selftest(void)
{
    const uint32_t DT = 100;
    const uint32_t LIMITE = EXPEDIENTE_DURACAO_MS + 10000;
    threat_state_t st;
    uint32_t t;
    int mit, perdas;

    ESP_LOGI(TAG, "=== SIM loop A: mitiga sempre ===");
    gamestate_reset(); threat_init();
    t = 0; mit = 0; perdas = 0;
    while (gamestate_get_clock_minutes() < HORA_FIM_JOGO_MIN &&
           gamestate_get_vidas() > 0 && t < LIMITE) {
        gamestate_tick(DT);
        if (threat_tick(DT)) { gamestate_perder_vida(); perdas++; }
        if (threat_get_active(&st)) {
            if (threat_mitigate(threat_carta_correta(st.tipo)) == DEFESA_CORRETO) mit++;
        }
        t += DT;
    }
    ESP_LOGI(TAG, "  A: mitigados=%d perdas=%d vidas=%u relogio=%umin -> %s",
             mit, perdas, gamestate_get_vidas(), gamestate_get_clock_minutes(),
             gamestate_get_vidas() > 0 ? "VITORIA" : "DERROTA");

    ESP_LOGI(TAG, "=== SIM loop B: nunca mitiga ===");
    gamestate_reset(); threat_init();
    t = 0; perdas = 0;
    while (gamestate_get_clock_minutes() < HORA_FIM_JOGO_MIN &&
           gamestate_get_vidas() > 0 && t < LIMITE) {
        gamestate_tick(DT);
        if (threat_tick(DT)) { gamestate_perder_vida(); perdas++; }
        t += DT;
    }
    ESP_LOGI(TAG, "  B: perdas=%d vidas=%u relogio=%umin -> %s",
             perdas, gamestate_get_vidas(), gamestate_get_clock_minutes(),
             gamestate_get_vidas() > 0 ? "VITORIA" : "DERROTA");

    gamestate_reset(); threat_init();   /* limpa pra run real */
}

/* === Jogador-fantasma (DEV — so roda com engine_set_test_mode(true)) ===== */

static void ghost_inject_button(uint8_t btn)
{
    const fsm_event_t fev = {
        .kind = FSM_EVT_BUTTON,
        .payload.button.id    = btn,
        .payload.button.state = 1,   /* BTN_PRESSED */
    };
    xQueueSend(s_event_queue, &fev, 0);
}

/* Dirige o loop sozinho e loga tudo: pula pro gameplay e, quando ha ataque
 * ativo, avanca o terminal (Y -> A -> X carta correta) pra mitigar. No
 * GAME_OVER loga o resultado e reinicia. Valida FSM + threat + matriz +
 * vitoria/derrota sem tocar fisicamente no aparelho. */
static void ghost_player_task(void *pv)
{
    (void)pv;
    vTaskDelay(pdMS_TO_TICKS(3000));   /* deixa o boot assentar */
    ESP_LOGW(TAG, "[GHOST] === jogador-fantasma iniciado ===");

    /* Splash/menu usam button-peek (nao a queue), entao pula direto pro jogo. */
    fsm_set_state(GAME_STATE_GAMEPLAY);
    fsm_set_player_at_equipment(true);

    uint16_t last_min = 0xFFFF;
    while (1) {
        const game_state_t macro = fsm_get_state();

        if (macro == GAME_STATE_GAME_OVER) {
            const uint16_t m = gamestate_get_clock_minutes();
            ESP_LOGW(TAG, "[GHOST] FIM: %s | vidas=%u | relogio=%02u:%02u",
                     gamestate_get_result() == RESULT_VITORIA ? "VITORIA" : "DERROTA",
                     gamestate_get_vidas(), m / 60, m % 60);
            vTaskDelay(pdMS_TO_TICKS(2500));
            ESP_LOGW(TAG, "[GHOST] reiniciando run...");
            ghost_inject_button(BTN_A);   /* GAME_OVER: A = retry -> GAMEPLAY */
            vTaskDelay(pdMS_TO_TICKS(800));
            fsm_set_player_at_equipment(true);
            last_min = 0xFFFF;
            continue;
        }

        if (macro == GAME_STATE_GAMEPLAY) {
            fsm_set_player_at_equipment(true);

            const uint16_t min = gamestate_get_clock_minutes();
            if (min != last_min) {
                threat_state_t ts;
                const bool atk = threat_get_active(&ts);
                ESP_LOGI(TAG, "[GHOST] %02u:%02u | vidas=%u | ataque=%s",
                         min / 60, min % 60, gamestate_get_vidas(),
                         atk ? ataque_nome(ts.tipo) : "nenhum");
                last_min = min;
            }

            threat_state_t ts;
            if (threat_get_active(&ts)) {
                switch (fsm_get_gameplay_substate()) {
                    case GAMEPLAY_SUB_EXPLORANDO:      ghost_inject_button(BTN_Y); break;
                    case GAMEPLAY_SUB_TERMINAL_ABERTO: ghost_inject_button(BTN_A); break;
                    case GAMEPLAY_SUB_WAITING_CARD:    ghost_inject_button(BTN_X); break;
                    default: break;   /* ACTION_LOCK / SYSTEM_DEPLOY: aguarda */
                }
            }
        }
        vTaskDelay(pdMS_TO_TICKS(400));
    }
}

/* === Arco-iris verde/amarelo/vermelho no MENU ============================ */

static inline uint8_t lerp8(uint8_t a, uint8_t b, uint8_t t)
{
    return (uint8_t)((int16_t)a + ((int16_t)b - (int16_t)a) * (int16_t)t / 255);
}

/* Cicla suavemente entre verde, amarelo e vermelho (cores de alerta do jogo).
 * Cada LED defasado uma fase: os 3 ficam visiveis simultaneamente.
 * Ciclo completo: 2,4 s (3 fases x 800 ms). */
static void rainbow_leds_tick(uint32_t dt_ms)
{
    static const uint8_t COLORS[3][3] = {
        {   0, 100,   0 },   /* verde    */
        { 100,  80,   0 },   /* amarelo  */
        { 100,   0,   0 },   /* vermelho */
    };
    static uint32_t s_ms = 0;
    s_ms += dt_ms;

    const uint32_t PHASE_MS = 800u;
    const uint32_t cycle    = s_ms % (PHASE_MS * 3u);
    const uint8_t  from     = (uint8_t)(cycle / PHASE_MS);
    const uint8_t  to       = (from + 1u) % 3u;
    const uint8_t  t        = (uint8_t)(cycle % PHASE_MS * 255u / PHASE_MS);

    for (uint8_t i = 0; i < WS2812_LED_COUNT; i++) {
        const uint8_t fi = (from + i) % 3u;
        const uint8_t ti = (to   + i) % 3u;
        ws2812_hal_set_pixel(i,
            LED_SCALE(lerp8(COLORS[fi][0], COLORS[ti][0], t)),
            LED_SCALE(lerp8(COLORS[fi][1], COLORS[ti][1], t)),
            LED_SCALE(lerp8(COLORS[fi][2], COLORS[ti][2], t)));
    }
    ws2812_hal_refresh();
}

static void engine_task(void *pv)
{
    (void)pv;

    /* Subscreve esta task ao Task WDT (configurado em sdkconfig com timeout
     * 5s). engine_task pode bloquear em xQueueReceive por ENGINE_TICK_PERIOD_MS
     * (100ms), entao o reset a cada iteracao da folga gigante (50x). Se a FSM
     * travar em um handler, WDT panic deixa rastro no log. */
    ESP_ERROR_CHECK(esp_task_wdt_add(NULL));

    TickType_t last_tick = xTaskGetTickCount();
    fsm_event_t evt;

    game_state_t    last_macro = fsm_get_state();
    gameplay_sala_t last_sala  = fsm_get_gameplay_sala();

    while (1) {
        esp_task_wdt_reset();

        /* Consome eventos com timeout pequeno; se nao houver, emite tick. */
        const TickType_t wait_ticks = pdMS_TO_TICKS(ENGINE_TICK_PERIOD_MS);
        if (xQueueReceive(s_event_queue, &evt, wait_ticks)) {
            fsm_handle_event(&evt);
        }

        /* Emite FSM_EVT_TICK a cada ENGINE_TICK_PERIOD_MS aproximadamente. */
        const TickType_t now = xTaskGetTickCount();
        const uint32_t dt_ms = pdTICKS_TO_MS(now - last_tick);
        if (dt_ms >= ENGINE_TICK_PERIOD_MS) {
            const fsm_event_t tick_evt = {
                .kind = FSM_EVT_TICK,
                .payload.tick.dt_ms = dt_ms,
            };
            fsm_handle_event(&tick_evt);
            update_debug_combo(dt_ms);
            gameplay_model_tick(dt_ms);   /* relogio + ataques + vitoria/derrota */
            gameplay_leds_tick(dt_ms);    /* LEDs de tarefa/perigo */
            nfc_poll_tick();              /* NFC — mitiga DDoS via carta fisica */
            if (fsm_get_state() == GAME_STATE_MENU) {
                rainbow_leds_tick(dt_ms);
            }
            last_tick = now;
        }

        /* Render sync: sob lv_lock, faz y_sort (se dirty) + debug_overlay
         * (se enabled). Chamado todo loop — early-returns internos garantem
         * custo zero em frames sem movimento e sem debug. */
        entity_render_sync();

        /* Observa mudancas. Macro mudou? troca tela. Sala mudou (dentro
         * de GAMEPLAY)? troca tela tambem. Sub-estado e observado pela
         * propria tela via getter. */
        const game_state_t    cur_macro = fsm_get_state();
        const gameplay_sala_t cur_sala  = fsm_get_gameplay_sala();
        if (cur_macro != last_macro) {
            /* Saiu do MENU ou do GAMEPLAY: apaga LEDs. */
            if (last_macro == GAME_STATE_MENU || last_macro == GAME_STATE_GAMEPLAY) {
                ws2812_hal_clear();
                ws2812_hal_refresh();
            }
            /* Entrada FRESCA em GAMEPLAY (vinda de MENU/SPLASH/GAME_OVER, nao
             * de PAUSE) = run nova: zera relogio + ataques. */
            if (cur_macro == GAME_STATE_GAMEPLAY && last_macro != GAME_STATE_PAUSE) {
                gamestate_reset();
                threat_init();
                screen_tarefa_amarela_reset();
                fsm_set_attack_active(false);
                s_led_anim   = LED_ANIM_NONE;
                s_req_defeat = false;
                s_req_mitig  = false;
                s_chaos_ms   = 0;
                ESP_LOGI(TAG, "[LOOP] nova run: relogio + ataques zerados");
            }
            sync_ui_to_macro(cur_macro);
            last_macro = cur_macro;
        } else if (cur_macro == GAME_STATE_GAMEPLAY && cur_sala != last_sala) {
            sync_gameplay_sala_to_ui(cur_sala);
        }
        last_sala = cur_sala;
    }
}

esp_err_t engine_init(void)
{
    if (s_initialized) {
        return ESP_OK;
    }

    s_event_queue = xQueueCreate(ENGINE_QUEUE_DEPTH, sizeof(fsm_event_t));
    ESP_RETURN_ON_FALSE(s_event_queue != NULL, ESP_ERR_NO_MEM, TAG, "queue alloc failed");

    ESP_RETURN_ON_ERROR(fsm_init(), TAG, "fsm_init failed");
    ESP_RETURN_ON_ERROR(entity_pool_init(), TAG, "entity_pool_init failed");
    y_sort_init();
    gamestate_init();
    threat_init();
    fsm_set_card_resolver(engine_card_resolver);
    defense_matrix_selftest();   /* loga a matriz no boot (validacao remota) */
    gameplay_sim_selftest();     /* simula o loop no boot (validacao remota) */

    {
        const esp_err_t nfc_err = nfc_hal_init();
        if (nfc_err == ESP_OK) {
            s_nfc_initialized = true;
            ESP_LOGI(TAG, "NFC HAL inicializado");
        } else {
            ESP_LOGW(TAG, "NFC HAL falhou (%s) — NFC desabilitado", esp_err_to_name(nfc_err));
        }
    }

    const esp_err_t ui_err = ui_init();
    if (ui_err != ESP_OK) {
        ESP_LOGE(TAG, "ui_init falhou (%s) — abortando engine_init", esp_err_to_name(ui_err));
        return ui_err;
    }
    ui_show_splash();

    s_initialized = true;
    ESP_LOGI(TAG, "engine_init OK");
    return ESP_OK;
}

esp_err_t engine_start(void)
{
    ESP_RETURN_ON_FALSE(s_initialized, ESP_ERR_INVALID_STATE, TAG, "chame engine_init antes");
    if (s_task != NULL) {
        return ESP_OK;
    }

    if (s_nfc_initialized) {
        nfc_hal_start_scanning();
        ESP_LOGI(TAG, "NFC scanning iniciado");
    }

    const BaseType_t rd = xTaskCreate(button_reader_task, "btn_reader",
                                       2560, NULL, 5, NULL);
    ESP_RETURN_ON_FALSE(rd == pdPASS, ESP_ERR_NO_MEM, TAG, "btn_reader spawn failed");

    const BaseType_t ok = xTaskCreatePinnedToCore(engine_task, "engine",
                                                   ENGINE_TASK_STACK, NULL,
                                                   ENGINE_TASK_PRIO, &s_task,
                                                   ENGINE_TASK_CORE);
    ESP_RETURN_ON_FALSE(ok == pdPASS, ESP_ERR_NO_MEM, TAG, "engine task spawn failed");

    if (s_test_mode) {
        ESP_LOGW(TAG, "[GHOST] modo teste ON — criando jogador-fantasma");
        xTaskCreate(ghost_player_task, "ghost", 3072, NULL, 4, NULL);
    }

    ESP_LOGI(TAG, "engine_start OK (core=%d, prio=%d)", ENGINE_TASK_CORE, ENGINE_TASK_PRIO);
    return ESP_OK;
}
