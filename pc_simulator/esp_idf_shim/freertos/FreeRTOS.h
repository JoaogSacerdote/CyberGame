#pragma once
#include <stdint.h>
#include <stdbool.h>

typedef uint32_t TickType_t;
typedef int32_t  BaseType_t;
typedef uint32_t UBaseType_t;

#define pdTRUE    ((BaseType_t)1)
#define pdFALSE   ((BaseType_t)0)
#define pdPASS    pdTRUE
#define pdFAIL    pdFALSE

#define portMAX_DELAY        ((TickType_t)0xFFFFFFFFu)
#define portTICK_PERIOD_MS   ((TickType_t)1u)
#define portTICK_RATE_MS     portTICK_PERIOD_MS

static inline TickType_t pdMS_TO_TICKS(uint32_t ms) { return (TickType_t)ms; }
static inline uint32_t   pdTICKS_TO_MS(TickType_t t){ return (uint32_t)t; }
