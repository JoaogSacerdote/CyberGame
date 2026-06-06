#pragma once

#include <stdbool.h>
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
void           fsm_set_player_at_equipment(bool at);

/* Resolver de carta no terminal (registrado pelo engine, que tem a matriz +
 * o ataque ativo). A FSM chama com mock_card (0 = carta correta, 1 = carta
 * errada — placeholder ate a leitura NFC real). Retorno:
 *   0  = mitigou (CORRETO)
 *   1  = sem efeito (INUTIL)
 *   2  = agravou (AGRAVA)
 *  -1  = nao ha ataque ativo
 * Desacopla a FSM do engine (sem dependencia circular). */
typedef int (*fsm_card_resolver_t)(int mock_card);
void           fsm_set_card_resolver(fsm_card_resolver_t cb);

#ifdef __cplusplus
}
#endif
