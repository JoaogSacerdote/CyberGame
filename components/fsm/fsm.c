#include "fsm.h"

#include "esp_log.h"

static const char *TAG = "FSM";

static game_state_t s_current = GAME_STATE_SPLASH;

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

esp_err_t fsm_init(void)
{
    s_current = GAME_STATE_SPLASH;
    ESP_LOGI(TAG, "fsm init -> %s", state_name(s_current));
    return ESP_OK;
}

game_state_t fsm_get_state(void)
{
    return s_current;
}

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
    s_current = new_state;
}

void fsm_handle_event(const fsm_event_t *evt)
{
    /* Etapa A: stub que so loga. Transicoes reais entram nas etapas C-E
     * conforme cada estado ganha sua sub-FSM. */
    if (evt == NULL) {
        return;
    }
    switch (evt->kind) {
        case FSM_EVT_BUTTON:
            ESP_LOGD(TAG, "[%s] BUTTON id=%u state=%u",
                     state_name(s_current), evt->payload.button.id, evt->payload.button.state);
            break;
        case FSM_EVT_JOYSTICK:
            ESP_LOGD(TAG, "[%s] JOYSTICK x=%d y=%d",
                     state_name(s_current), evt->payload.joystick.x, evt->payload.joystick.y);
            break;
        case FSM_EVT_NFC:
            ESP_LOGD(TAG, "[%s] NFC uid_len=%u", state_name(s_current), evt->payload.nfc.uid_len);
            break;
        case FSM_EVT_TICK:
            /* Silencioso por padrao — ticks chegam ~10x/s. */
            break;
    }
}
