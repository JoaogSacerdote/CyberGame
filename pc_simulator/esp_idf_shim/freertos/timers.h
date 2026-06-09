#pragma once
#include "FreeRTOS.h"
#include <stdbool.h>

typedef void *TimerHandle_t;
typedef void (*TimerCallbackFunction_t)(TimerHandle_t xTimer);

static inline TimerHandle_t xTimerCreate(const char *n, TickType_t p, bool reload,
                                          void *id, TimerCallbackFunction_t cb) {
    (void)n; (void)p; (void)reload; (void)id; (void)cb; return NULL;
}
static inline BaseType_t xTimerStart(TimerHandle_t t, TickType_t w) {
    (void)t; (void)w; return pdFALSE;
}
static inline BaseType_t xTimerStop(TimerHandle_t t, TickType_t w) {
    (void)t; (void)w; return pdFALSE;
}
static inline BaseType_t xTimerReset(TimerHandle_t t, TickType_t w) {
    (void)t; (void)w; return pdFALSE;
}
static inline BaseType_t xTimerResetFromISR(TimerHandle_t t, BaseType_t *hp) {
    (void)t; (void)hp; return pdFALSE;
}
static inline void *pvTimerGetTimerID(TimerHandle_t t) {
    (void)t; return NULL;
}
