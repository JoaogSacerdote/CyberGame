#pragma once

#include "esp_err.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * melody_player — reprodutor não-bloqueante de melodias para buzzer passivo.
 *
 * Usa buzzer_hal por baixo (buzzer_hal_init deve ter sido chamado antes).
 * O avanço de nota é acionado por esp_timer one-shot: nenhuma task extra.
 *
 * Thread-safety: play/stop podem ser chamados de qualquer task; o timer
 * callback roda no contexto esp_timer e não bloqueia o chamador.
 */

typedef enum {
    MELODY_RECEPCAO   = 0,
    MELODY_ESCRITORIO = 1,
    MELODY_ATAQUE     = 2,
    MELODY_NONE       = 3,
} melody_id_t;

/* Inicializa o player (cria o esp_timer interno). Idempotente. */
esp_err_t melody_player_init(void);

/* Inicia ou troca a melodia em loop ou one-shot.
 * Chamar com o mesmo id que já está tocando é no-op (não reinicia). */
void melody_player_play(melody_id_t id, bool loop);

/* Para imediatamente e silencia o buzzer. */
void melody_player_stop(void);

/* Retorna o id da melodia atual (MELODY_NONE se parado). */
melody_id_t melody_player_current(void);

#ifdef __cplusplus
}
#endif
