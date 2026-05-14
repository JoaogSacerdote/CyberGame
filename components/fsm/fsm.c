#include "fsm.h"
#include "fsm_gameplay.h"

#include "esp_log.h"
#include "button_hal.h"

static const char *TAG = "FSM";

/* Constantes de fase (em ms). Replicam game_config.h sem incluir o header
 * — a FSM nao deve ter dependencia em engine/. Os valores tem que bater. */
#define FSM_ACTION_LOCK_MS    1500
#define FSM_SYSTEM_DEPLOY_MS  4000

static game_state_t        s_current     = GAME_STATE_SPLASH;
static gameplay_substate_t s_sub         = GAMEPLAY_SUB_EXPLORANDO;
static gameplay_sala_t     s_sala        = GAMEPLAY_SALA_RECEPCAO;
static uint32_t            s_phase_ms    = 0;   /* tempo decorrido no sub-estado atual */

static const char *state_name(game_state_t s)
{
    switch (s) {
        case GAME_STATE_SPLASH:        return "SPLASH";
        case GAME_STATE_MENU:          return "MENU";
        case GAME_STATE_GAMEPLAY:      return "GAMEPLAY";
        case GAME_STATE_PAUSE:         return "PAUSE";
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

gameplay_sala_t fsm_get_gameplay_sala(void) { return s_sala; }

void fsm_set_gameplay_sala(gameplay_sala_t sala)
{
    if (sala >= GAMEPLAY_SALA_MAX || sala == s_sala) return;
    ESP_LOGI(TAG, "[GAMEPLAY] sala %s -> %s",
             fsm_gameplay_sala_name(s_sala), fsm_gameplay_sala_name(sala));
    s_sala = sala;
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
    s_current  = GAME_STATE_SPLASH;
    s_sub      = GAMEPLAY_SUB_EXPLORANDO;
    s_sala     = GAMEPLAY_SALA_RECEPCAO;
    s_phase_ms = 0;
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
    /* Entrada em GAMEPLAY reseta sub-FSM. Sala so reseta pra RECEPCAO
     * se veio de MENU/SPLASH; se veio de PAUSE, preserva sala atual. */
    if (new_state == GAME_STATE_GAMEPLAY) {
        s_sub      = GAMEPLAY_SUB_EXPLORANDO;
        s_phase_ms = 0;
        if (prev == GAME_STATE_MENU || prev == GAME_STATE_SPLASH) {
            s_sala = GAMEPLAY_SALA_RECEPCAO;
        }
    }
}

/* Handler do sub-FSM de gameplay. Botoes A/B/X/Y disparam transicoes de
 * sub-estado; START sobe macro pra PAUSE; B em EXPLORANDO sai pro MENU.
 * NFC real e timeline entram nos sub-blocos seguintes da Etapa C. */
static void gameplay_handle_event(const fsm_event_t *evt)
{
    if (evt->kind == FSM_EVT_TICK) {
        s_phase_ms += evt->payload.tick.dt_ms;
        switch (s_sub) {
            case GAMEPLAY_SUB_ACTION_LOCK:
                if (s_phase_ms >= FSM_ACTION_LOCK_MS) {
                    set_sub(GAMEPLAY_SUB_SYSTEM_DEPLOY);
                }
                break;
            case GAMEPLAY_SUB_SYSTEM_DEPLOY:
                if (s_phase_ms >= FSM_SYSTEM_DEPLOY_MS) {
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
            if (btn == BTN_Y) set_sub(GAMEPLAY_SUB_TERMINAL_ABERTO);
            else if (btn == BTN_B) fsm_set_state(GAME_STATE_MENU);
            break;
        case GAMEPLAY_SUB_TERMINAL_ABERTO:
            if (btn == BTN_A) set_sub(GAMEPLAY_SUB_WAITING_CARD);
            else if (btn == BTN_B) set_sub(GAMEPLAY_SUB_EXPLORANDO);
            break;
        case GAMEPLAY_SUB_WAITING_CARD:
            /* X = mock de leitura NFC enquanto nfc_config.h nao tem UIDs reais. */
            if (btn == BTN_X) set_sub(GAMEPLAY_SUB_ACTION_LOCK);
            else if (btn == BTN_B) set_sub(GAMEPLAY_SUB_TERMINAL_ABERTO);
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
        default:
            /* SPLASH/MENU/RANKING/CREDITOS: por enquanto a UI dirige a
             * navegacao via peek. Quando migrarem pra FSM, entram aqui. */
            break;
    }
}
