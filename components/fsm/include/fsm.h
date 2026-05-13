#pragma once

#include "esp_err.h"
#include "fsm_states.h"
#include "fsm_events.h"

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t      fsm_init(void);
game_state_t   fsm_get_state(void);
void           fsm_set_state(game_state_t new_state);
void           fsm_handle_event(const fsm_event_t *evt);

#ifdef __cplusplus
}
#endif
