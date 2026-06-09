#include "pmu.h"
#include <stdlib.h>

esp_err_t       pmu_init(void)                         { return ESP_OK; }
pmu_boot_mode_t pmu_check_boot_mode(void)              { return PMU_BOOT_NORMAL; }
void            pmu_enter_deep_sleep(void)             { exit(0); }
void            pmu_shutdown_monitor_task(void *pv)    { (void)pv; }
