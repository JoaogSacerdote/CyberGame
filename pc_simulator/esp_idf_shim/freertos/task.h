#pragma once
#include "FreeRTOS.h"

typedef void (*TaskFunction_t)(void *pv);
typedef void *TaskHandle_t;

static inline void vTaskDelay(TickType_t ticks) { (void)ticks; }
static inline TickType_t xTaskGetTickCount(void) { return 0; }

static inline BaseType_t xTaskCreate(TaskFunction_t fn, const char *name,
                                      uint32_t stack, void *param,
                                      UBaseType_t prio, TaskHandle_t *handle) {
    (void)fn; (void)name; (void)stack; (void)param; (void)prio; (void)handle;
    return pdTRUE;
}

static inline BaseType_t xTaskCreatePinnedToCore(TaskFunction_t fn, const char *name,
                                                   uint32_t stack, void *param,
                                                   UBaseType_t prio, TaskHandle_t *handle,
                                                   int core) {
    (void)core;
    return xTaskCreate(fn, name, stack, param, prio, handle);
}
