#pragma once
#include "FreeRTOS.h"

typedef void *SemaphoreHandle_t;

#define xSemaphoreCreateMutex()           NULL
#define xSemaphoreCreateBinary()          NULL
#define xSemaphoreTake(sem, t)            pdTRUE
#define xSemaphoreGive(sem)               pdTRUE
#define xSemaphoreGiveFromISR(sem, hp)    pdTRUE
#define xSemaphoreTakeFromISR(sem, hp)    pdTRUE
#define vSemaphoreDelete(sem)             do {} while(0)
