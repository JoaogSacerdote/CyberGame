#include "fsm.h"
#include "fsm_gameplay.h"

#include "esp_log.h"
#include "button_hal.h"
#include "gamestate.h"
#include "game_config.h"

static const char *TAG = "FSM";

static game_state_t        s_current             = GAME_STATE_SPLASH;
static gameplay_substate_t s_sub                 = GAMEPLAY_SUB_EXPLORANDO;
static gameplay_sala_t     s_sala                = GAMEPLAY_SALA_RECEPCAO;
static gameplay_sala_t     s_sala_prev           = GAMEPLAY_SALA_RECEPCAO;
static uint32_t            s_phase_ms            = 0;   /* tempo decorrido no sub-estado atual */
static bool                s_player_at_equipment = false; /* setado pelo tick do screen ativo */
static bool                s_attack_active       = false; /* ataque vermelho ativo (setado pelo engine) */
static fsm_card_resolver_t s_card_resolver       = NULL;  /* registrado pelo engine */

void fsm_set_card_resolver(fsm_card_resolver_t cb) { s_card_resolver = cb; }

static const char *state_name(game_state_t s)
{
    switch (s) {
        case GAME_STATE_SPLASH:        return "SPLASH";
        case GAME_STATE_MENU:          return "MENU";
        case GAME_STATE_GAMEPLAY:      return "GAMEPLAY";
        case GAME_STATE_PAUSE:         return "PAUSE";
        case GAME_STATE_GAME_OVER:     return "GAME_OVER";
        case GAME_STATE_RANKING_VIEW:  return "RANKING";
        case GAME_STATE_CREDITOS:      return "CREDITOS";
        default:                       return "INVALID";
    }
}

const char *fsm_gameplay_substate_name(gameplay_substate_t s)
{
    switch (s) {
        case GAMEPLAY_SUB_EXPLORANDO:      return "EXPLORANDO";
        case GAMEPLAY_SUB_TERMINAL_ABERTO: return "TERMINAL_ABERTO";
        case GAMEPLAY_SUB_WAITING_CARD:    return "WAITING_CARD";
        case GAMEPLAY_SUB_ACTION_LOCK:     return "ACTION_LOCK";
        case GAMEPLAY_SUB_SYSTEM_DEPLOY:   return "SYSTEM_DEPLOY";
        default:                           return "INVALID";
    }
}

const char *fsm_gameplay_sala_name(gameplay_sala_t s)
{
    switch (s) {
        case GAMEPLAY_SALA_RECEPCAO: return "RECEPCAO";
        case GAMEPLAY_SALA_EMPRESA:  return "EMPRESA";
        default:                     return "INVALID";
    }
}

gameplay_sala_t fsm_get_gameplay_sala(void)      { return s_sala; }
gameplay_sala_t fsm_get_gameplay_sala_prev(void) { return s_sala_prev; }

void fsm_set_player_at_equipment(bool at)        { s_player_at_equipment = at; }
bool fsm_get_player_at_equipment(void)           { return s_player_at_equipment; }

void fsm_set_attack_active(bool active)          { s_attack_active = active; }
bool fsm_get_attack_active(void)                 { return s_attack_active; }

void fsm_set_gameplay_sala(gameplay_sala_t sala)
{
    if (sala >= GAMEPLAY_SALA_MAX || sala == s_sala) return;
    ESP_LOGI(TAG, "[GAMEPLAY] sala %s -> %s",
             fsm_gameplay_sala_name(s_sala), fsm_gameplay_sala_name(sala));
    s_sala_prev = s_sala;
    s_sala = sala;
    /* Trocar de sala invalida automaticamente "estou no equipamento" — a
     * nova sala precisa reafirmar via fsm_set_player_at_equipment(true) se
     * spawnar dentro de um gatilho de equipamento. Sem isso, salas sem
     * equipamento (ex.: Recepcao) precisavam chamar set(false) toda
     * iteracao como defesa, o que era fragil. */
    s_player_at_equipment = false;
}

static void set_sub(gameplay_substate_t next)
{
    if (next == s_sub) return;
    ESP_LOGI(TAG, "[GAMEPLAY] sub %s -> %s",
             fsm_gameplay_substate_name(s_sub),
             fsm_gameplay_substate_name(next));
    s_sub      = next;
    s_phase_ms = 0;
}

esp_err_t fsm_init(void)
{
    s_current             = GAME_STATE_SPLASH;
    s_sub                 = GAMEPLAY_SUB_EXPLORANDO;
    s_sala                = GAMEPLAY_SALA_RECEPCAO;
    s_sala_prev           = GAMEPLAY_SALA_RECEPCAO;
    s_phase_ms            = 0;
    s_player_at_equipment = false;
    s_attack_active       = false;
    ESP_LOGI(TAG, "fsm init -> %s", state_name(s_current));
    return ESP_OK;
}

game_state_t fsm_get_state(void)            { return s_current; }
gameplay_substate_t fsm_get_gameplay_substate(void) { return s_sub; }

void fsm_set_state(game_state_t new_state)
{
    if (new_state >= GAME_STATE_MAX) {
        ESP_LOGW(TAG, "fsm_set_state ignorado: estado invalido %d", new_state);
        return;
    }
    if (new_state == s_current) {
        return;
    }
    ESP_LOGI(TAG, "transicao %s -> %s", state_name(s_current), state_name(new_state));
    const game_state_t prev = s_current;
    s_current = new_state;
    /* Entrada em GAMEPLAY vinda de MENU/SPLASH/GAME_OVER e uma run nova:
     * zera tudo. Vinda de PAUSE e retomada: preserva sub-FSM, phase_ms e
     * sala — o jogador continua exatamente de onde pausou. */
    if (new_state == GAME_STATE_GAMEPLAY &&
        (prev == GAME_STATE_MENU ||
         prev == GAME_STATE_SPLASH ||
         prev == GAME_STATE_GAME_OVER)) {
        s_sub                 = GAMEPLAY_SUB_EXPLORANDO;
        s_phase_ms            = 0;
        s_sala                = GAMEPLAY_SALA_RECEPCAO;
        s_player_at_equipment = false;
        s_attack_active       = false;
    }

    /* Entrada em GAME_OVER zera phase_ms pra contar o timeout de 30s. */
    if (new_state == GAME_STATE_GAME_OVER) {
        s_phase_ms = 0;
    }
}

/* Handler do sub-FSM de gameplay. Botoes A/B/X/Y disparam transicoes de
 * sub-estado; START sobe macro pra PAUSE.
 * NFC real e timeline entram nos sub-blocos seguintes da Etapa C. */
static void gameplay_handle_event(const fsm_event_t *evt)
{
    if (evt->kind == FSM_EVT_TICK) {
        s_phase_ms += evt->payload.tick.dt_ms;
        switch (s_sub) {
            case GAMEPLAY_SUB_ACTION_LOCK:
                if (s_phase_ms >= ACTION_LOCK_MS) {
                    set_sub(GAMEPLAY_SUB_SYSTEM_DEPLOY);
                }
                break;
            case GAMEPLAY_SUB_SYSTEM_DEPLOY:
                if (s_phase_ms >= SYSTEM_DEPLOY_MS) {
                    set_sub(GAMEPLAY_SUB_EXPLORANDO);
                }
                break;
            default:
                break;
        }
        return;
    }

    if (evt->kind != FSM_EVT_BUTTON) return;
    if (evt->payload.button.state != BTN_PRESSED) return;
    const uint8_t btn = evt->payload.button.id;

    /* START em qualquer sub-estado de gameplay -> PAUSE. */
    if (btn == BTN_START) {
        fsm_set_state(GAME_STATE_PAUSE);
        return;
    }

    switch (s_sub) {
        case GAMEPLAY_SUB_EXPLORANDO:
            /* Y so abre o terminal se o player estiver encostado em um
             * gatilho de equipamento. Quem mantem a flag e o tick do screen
             * ativo via fsm_set_player_at_equipment(). */
            if (btn == BTN_Y && s_player_at_equipment) set_sub(GAMEPLAY_SUB_TERMINAL_ABERTO);
            break;
        case GAMEPLAY_SUB_TERMINAL_ABERTO:
            if (btn == BTN_A) set_sub(GAMEPLAY_SUB_WAITING_CARD);
            else if (btn == BTN_B) set_sub(GAMEPLAY_SUB_EXPLORANDO);
            break;
        case GAMEPLAY_SUB_WAITING_CARD:
            /* X = "escaneia carta correta" (mock), Y = "carta errada" (mock).
             * O engine resolve via matriz contra o ataque ativo (resolver).
             * A perda de vida NAO acontece aqui — quem decide e o threat_tick
             * (engine) quando o ataque expira sem mitigacao.
             * B = abortar / voltar pro terminal. */
            if (btn == BTN_X || btn == BTN_Y) {
                const int mock_card = (btn == BTN_X) ? 0 : 1;
                const int res = (s_card_resolver != NULL) ? s_card_resolver(mock_card) : -1;
                if (res == 0) {            /* CORRETO -> mitiga, segue pro deploy */
                    set_sub(GAMEPLAY_SUB_ACTION_LOCK);
                } else if (res == 2) {     /* AGRAVA */
                    ESP_LOGW(TAG, "[GAMEPLAY] carta AGRAVOU o ataque");
                } else if (res == 1) {     /* INUTIL */
                    ESP_LOGW(TAG, "[GAMEPLAY] carta inutil (so perdeu tempo)");
                } else {                   /* -1: nenhum ataque ativo */
                    ESP_LOGI(TAG, "[GAMEPLAY] sem ataque ativo pra mitigar");
                    set_sub(GAMEPLAY_SUB_TERMINAL_ABERTO);
                }
            } else if (btn == BTN_B) {
                set_sub(GAMEPLAY_SUB_TERMINAL_ABERTO);
            }
            break;
        case GAMEPLAY_SUB_ACTION_LOCK:
        case GAMEPLAY_SUB_SYSTEM_DEPLOY:
            /* Fases automaticas — botoes ignorados (B aborto entra no sub-bloco
             * de tarefas, junto com personagem e attack_matrix). */
            break;
        default:
            break;
    }
}

/* Handler de PAUSE: START retoma, B sai pro menu. */
static void pause_handle_event(const fsm_event_t *evt)
{
    if (evt->kind != FSM_EVT_BUTTON) return;
    if (evt->payload.button.state != BTN_PRESSED) return;
    const uint8_t btn = evt->payload.button.id;
    if (btn == BTN_START) fsm_set_state(GAME_STATE_GAMEPLAY);
    else if (btn == BTN_B) fsm_set_state(GAME_STATE_MENU);
}

/* Handler de GAME_OVER (Tela de Demissao): A tenta de novo, B vai pro menu,
 * 30s ocioso -> volta pra splash automaticamente. */
static void gameover_handle_event(const fsm_event_t *evt)
{
    if (evt->kind == FSM_EVT_TICK) {
        s_phase_ms += evt->payload.tick.dt_ms;
        if (s_phase_ms >= TIMEOUT_TELA_FINAL_MS) {
            fsm_set_state(GAME_STATE_SPLASH);
        }
        return;
    }
    if (evt->kind != FSM_EVT_BUTTON) return;
    if (evt->payload.button.state != BTN_PRESSED) return;
    const uint8_t btn = evt->payload.button.id;
    if (btn == BTN_A) fsm_set_state(GAME_STATE_GAMEPLAY);  /* retry */
    else if (btn == BTN_B) fsm_set_state(GAME_STATE_MENU); /* sair */
}

void fsm_handle_event(const fsm_event_t *evt)
{
    if (evt == NULL) return;
    switch (s_current) {
        case GAME_STATE_GAMEPLAY:
            gameplay_handle_event(evt);
            break;
        case GAME_STATE_PAUSE:
            pause_handle_event(evt);
            break;
        case GAME_STATE_GAME_OVER:
            gameover_handle_event(evt);
            break;
        default:
            /* SPLASH/MENU/RANKING/CREDITOS: por enquanto a UI dirige a
             * navegacao via peek. Quando migrarem pra FSM, entram aqui. */
            break;
    }
}
