#include <stdint.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

#include "pmu.h"
#include "button_hal.h"

static const char *TAG = "APP_MAIN";

static void button_logger_task(void *pv)
{
    (void)pv;
    static const char *names[BTN_MAX_COUNT] = { "A", "B", "X", "Y" };
    button_event_t ev;
    while (1) {
        if (button_hal_get_event(&ev, UINT32_MAX)) {
            ESP_LOGI(TAG, "Botao %s %s",
                     names[ev.id],
                     ev.state == BTN_PRESSED ? "pressionado" : "solto");
        }
    }
}

void app_main(void)
{
    ESP_ERROR_CHECK(pmu_init());

    ESP_LOGI(TAG, "Avaliando Power Latch...");
    if (!pmu_check_boot_hold()) {
        ESP_LOGW(TAG, "Botao solto prematuramente. Voltando ao Deep Sleep.");
        pmu_enter_deep_sleep();
    }
    ESP_LOGI(TAG, "Boot confirmado! Iniciando sistema...");

    xTaskCreate(pmu_shutdown_monitor_task, "pmu_monitor", 3072, NULL, 5, NULL);

    ESP_ERROR_CHECK(button_hal_init());
    xTaskCreate(button_logger_task, "btn_logger", 3072, NULL, 4, NULL);

    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
