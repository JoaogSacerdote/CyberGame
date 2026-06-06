#pragma once

#include <stdbool.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Liga/desliga o JOGADOR-FANTASMA (task de teste que dirige o loop e loga
 * tudo). Controlado pela variavel DEV_TEST_MODE em main/main.c — main chama
 * isto antes de engine_start. Default desligado. */
void engine_set_test_mode(bool enable);

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
