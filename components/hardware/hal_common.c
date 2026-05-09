#include "hal_common.h"

#include <stdbool.h>
#include "driver/gpio.h"

static bool s_isr_service_installed = false;

esp_err_t hal_isr_service_install_once(void)
{
    if (s_isr_service_installed) {
        return ESP_OK;
    }
    const esp_err_t err = gpio_install_isr_service(ESP_INTR_FLAG_IRAM);
    if (err == ESP_OK) {
        s_isr_service_installed = true;
    }
    return err;
}
