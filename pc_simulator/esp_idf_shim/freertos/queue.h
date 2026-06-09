#pragma once
#include "FreeRTOS.h"

typedef void *QueueHandle_t;

static inline QueueHandle_t xQueueCreate(uint32_t len, uint32_t size) {
    (void)len; (void)size; return NULL;
}
static inline BaseType_t xQueueSend(QueueHandle_t q, const void *item, TickType_t t) {
    (void)q; (void)item; (void)t; return pdFALSE;
}
static inline BaseType_t xQueueReceive(QueueHandle_t q, void *item, TickType_t t) {
    (void)q; (void)item; (void)t; return pdFALSE;
}
static inline BaseType_t xQueueSendFromISR(QueueHandle_t q, const void *item, BaseType_t *hp) {
    (void)q; (void)item; (void)hp; return pdFALSE;
}
static inline uint32_t xQueueGetLength(QueueHandle_t q) {
    (void)q; return 0;
}
