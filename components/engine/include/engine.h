#pragma once

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/* engine_init: inicializa fsm + ui + cria a queue de eventos.
 * Nao inicia a task ainda. Idempotente.
 */
esp_err_t engine_init(void);

/* engine_start: cria a game_task (Core 0) que consome eventos da queue
 * e tickа a FSM a cada ENGINE_TICK_PERIOD_MS. Retorna apos a task estar
 * rodando.
 */
esp_err_t engine_start(void);

#ifdef __cplusplus
}
#endif
