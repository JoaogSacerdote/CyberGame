#pragma once

#include <stdint.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    int8_t x;  /* -100 (esquerda) ... +100 (direita), 0 no centro */
    int8_t y;  /* -100 (baixo)    ... +100 (cima),    0 no centro */
} joystick_data_t;

esp_err_t       joystick_hal_init(void);

/* Snapshot do estado processado mais recente (thread-safe). Em caso de falha
 * de aquisicao do mutex, retorna {0, 0} ao inves de bloquear o chamador. */
joystick_data_t joystick_hal_get_state(void);

#ifdef __cplusplus
}
#endif
