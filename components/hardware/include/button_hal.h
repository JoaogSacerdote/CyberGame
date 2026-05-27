#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    BTN_A = 0,
    BTN_B,
    BTN_X,
    BTN_Y,
    BTN_START,    /* GPIO 13 dual-use: PMU le como REC durante boot, button_hal claim como START apos */
    BTN_MAX_COUNT,
} button_id_t;

typedef enum {
    BTN_RELEASED = 0,
    BTN_PRESSED  = 1,
} button_state_t;

typedef struct {
    button_id_t    id;
    button_state_t state;
} button_event_t;

esp_err_t      button_hal_init(void);

/* Bloqueia ate consumir um evento ou timeout. Use UINT32_MAX para esperar indefinidamente. */
bool           button_hal_get_event(button_event_t *event, uint32_t timeout_ms);

/* Estado estavel atual (nao-bloqueante, sem consumir fila). */
button_state_t button_hal_peek(button_id_t id);

#ifdef __cplusplus
}
#endif
