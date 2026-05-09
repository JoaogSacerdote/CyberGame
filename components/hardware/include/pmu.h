#pragma once

#include <stdbool.h>
#include "esp_err.h"
#include "driver/gpio.h"

#define PMU_PIN_PWR             GPIO_NUM_4
#define PMU_PIN_REC             GPIO_NUM_3
#define PMU_HOLD_BOOT_MS        2000
#define PMU_HOLD_SHUTDOWN_MS    4000

typedef enum {
    PMU_BOOT_ABORT     = 0,  /* PWR solto antes do hold completar — voltar para Deep Sleep */
    PMU_BOOT_NORMAL,         /* Apenas PWR segurado por 2s — fluxo normal do jogo */
    PMU_BOOT_RECOVERY,       /* PWR + REC segurados por 2s — modo gravador (USB MSC futuro) */
} pmu_boot_mode_t;

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t       pmu_init(void);
pmu_boot_mode_t pmu_check_boot_mode(void);
void            pmu_enter_deep_sleep(void);
void            pmu_shutdown_monitor_task(void *pvParameters);

#ifdef __cplusplus
}
#endif
