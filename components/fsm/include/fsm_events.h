#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    FSM_EVT_BUTTON = 0,    /* botao pressionado ou solto */
    FSM_EVT_JOYSTICK,      /* nova leitura significativa do joystick */
    FSM_EVT_NFC,           /* UID de carta lido */
    FSM_EVT_TICK,          /* tick periodico do engine (~100ms) */
} fsm_event_kind_t;

typedef struct {
    fsm_event_kind_t kind;
    union {
        struct {
            uint8_t id;        /* button_id_t cast para uint8_t (sem incluir button_hal.h aqui) */
            uint8_t state;     /* 0 = solto, 1 = pressionado */
        } button;
        struct {
            int8_t x;          /* -100..+100, sintonizado com joystick_data_t */
            int8_t y;
        } joystick;
        struct {
            uint8_t  uid[8];
            uint8_t  uid_len;
        } nfc;
        struct {
            uint32_t dt_ms;    /* tempo decorrido desde o ultimo tick */
        } tick;
    } payload;
} fsm_event_t;

#ifdef __cplusplus
}
#endif
