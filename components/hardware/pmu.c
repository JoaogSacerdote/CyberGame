#include "pmu.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "driver/rtc_io.h"
#include "esp_sleep.h"
#include "esp_check.h"
#include "esp_log.h"

static const char *TAG = "PMU";

esp_err_t pmu_init(void)
{
    /* Libera o pino do dominio RTC: necessario quando boot vem de wake-up
     * de Deep Sleep, senao gpio_config nao consegue reconfigurar o pino. */
    rtc_gpio_deinit(PMU_PIN_PWR);

    const gpio_config_t cfg = {
        .pin_bit_mask = (1ULL << PMU_PIN_PWR),
        .mode         = GPIO_MODE_INPUT,
        .pull_up_en   = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type    = GPIO_INTR_DISABLE,
    };

    ESP_RETURN_ON_ERROR(gpio_config(&cfg), TAG, "gpio_config failed for PMU_PIN_PWR");
    return ESP_OK;
}

bool pmu_check_boot_hold(void)
{
    /* Settle inicial para mascarar bounce mecanico do botao. */
    vTaskDelay(pdMS_TO_TICKS(100));

    const TickType_t step_ticks = pdMS_TO_TICKS(100);
    const int total_steps = PMU_HOLD_BOOT_MS / 100;

    for (int i = 0; i < total_steps; ++i) {
        if (gpio_get_level(PMU_PIN_PWR) != 0) {
            return false;
        }
        vTaskDelay(step_ticks);
    }
    return true;
}

void pmu_enter_deep_sleep(void)
{
    ESP_LOGW(TAG, "Aguardando soltura do botao para entrar em Deep Sleep...");
    while (gpio_get_level(PMU_PIN_PWR) == 0) {
        vTaskDelay(pdMS_TO_TICKS(50));
    }
    vTaskDelay(pdMS_TO_TICKS(200));

    /* Pull-up digital nao sobrevive ao Deep Sleep no ESP32-S3: sem migrar o
     * pino para o dominio RTC com pull-up ativo, GPIO 4 fica flutuando e
     * ruido pode disparar wake-ups espurios. */
    rtc_gpio_init(PMU_PIN_PWR);
    rtc_gpio_set_direction(PMU_PIN_PWR, RTC_GPIO_MODE_INPUT_ONLY);
    rtc_gpio_pullup_en(PMU_PIN_PWR);
    rtc_gpio_pulldown_dis(PMU_PIN_PWR);

    esp_err_t err = esp_sleep_enable_ext0_wakeup(PMU_PIN_PWR, 0);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_sleep_enable_ext0_wakeup falhou: %s. Reiniciando em vez de dormir sem wake-source.",
                 esp_err_to_name(err));
        vTaskDelay(pdMS_TO_TICKS(50));
        esp_restart();
    }

    esp_deep_sleep_start();
}

void pmu_shutdown_monitor_task(void *pvParameters)
{
    (void)pvParameters;

    const TickType_t step_ticks = pdMS_TO_TICKS(100);
    const int total_steps = PMU_HOLD_SHUTDOWN_MS / 100;
    int held_steps = 0;

    while (1) {
        if (gpio_get_level(PMU_PIN_PWR) == 0) {
            held_steps++;
            if (held_steps >= total_steps) {
                ESP_LOGI(TAG, "Hold de shutdown detectado (%d ms). Entrando em Deep Sleep.",
                         PMU_HOLD_SHUTDOWN_MS);
                pmu_enter_deep_sleep();
            }
        } else {
            held_steps = 0;
        }
        vTaskDelay(step_ticks);
    }
}
