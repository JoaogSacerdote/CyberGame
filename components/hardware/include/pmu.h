#pragma once

#include <stdbool.h>
#include "esp_err.h"
#include "driver/gpio.h"

#define PMU_PIN_PWR             GPIO_NUM_4
#define PMU_HOLD_BOOT_MS        2000
#define PMU_HOLD_SHUTDOWN_MS    4000

/* TODO: botao secundario para boot modifier (modo manutencao / recovery).
 * Pino indefinido — sera decidido se sera GPIO dedicado ou reaproveitamento
 * de um dos botoes de acao A/B/X/Y. Quando definido, trocar o retorno bool
 * de pmu_check_boot_hold por um enum pmu_boot_mode_t com 3 estados (abort,
 * operational, maintenance). */
/* #define PMU_PIN_REC          GPIO_NUM_NC */

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t pmu_init(void);
bool      pmu_check_boot_hold(void);
void      pmu_enter_deep_sleep(void);
void      pmu_shutdown_monitor_task(void *pvParameters);

#ifdef __cplusplus
}
#endif
