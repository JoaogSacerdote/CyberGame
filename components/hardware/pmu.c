#include "pmu.h"
#include "board_pins.h"

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
    /* Libera o PWR do dominio RTC: necessario quando boot vem de wake-up
     * de Deep Sleep, senao gpio_config nao consegue reconfigurar o pino. */
    rtc_gpio_deinit(BOARD_PIN_PMU_PWR);

    /* PWR e REC compartilham a mesma config: input pull-up, sem interrupcao.
     * Botoes ligados a GND, nivel LOW = pressionado. */
    const gpio_config_t cfg = {
        .pin_bit_mask = (1ULL << BOARD_PIN_PMU_PWR) | (1ULL << BOARD_PIN_PMU_REC),
        .mode         = GPIO_MODE_INPUT,
        .pull_up_en   = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type    = GPIO_INTR_DISABLE,
    };

    ESP_RETURN_ON_ERROR(gpio_config(&cfg), TAG, "gpio_config failed for PWR/REC");
    return ESP_OK;
}

pmu_boot_mode_t pmu_check_boot_mode(void)
{
    /* Settle inicial para mascarar bounce mecanico do botao. */
    vTaskDelay(pdMS_TO_TICKS(100));

    const TickType_t step_ticks = pdMS_TO_TICKS(100);
    const int total_steps = PMU_HOLD_BOOT_MS / 100;

    for (int i = 0; i < total_steps; ++i) {
        if (gpio_get_level(BOARD_PIN_PMU_PWR) != 0) {
            return PMU_BOOT_ABORT;
        }
        vTaskDelay(step_ticks);
    }

    /* PWR confirmado pelo hold de 2s. Amostra REC para decidir o modo:
     * REC pressionado no fim do hold = boot em modo gravador (RECOVERY). */
    if (gpio_get_level(BOARD_PIN_PMU_REC) == 0) {
        return PMU_BOOT_RECOVERY;
    }
    return PMU_BOOT_NORMAL;
}

void pmu_enter_deep_sleep(void)
{
    ESP_LOGW(TAG, "Aguardando soltura do botao para entrar em Deep Sleep...");
    while (gpio_get_level(BOARD_PIN_PMU_PWR) == 0) {
        vTaskDelay(pdMS_TO_TICKS(50));
    }
    vTaskDelay(pdMS_TO_TICKS(200));

    /* Pull-up digital nao sobrevive ao Deep Sleep no ESP32-S3: sem migrar o
     * pino para o dominio RTC com pull-up ativo, GPIO 14 (BOARD_PIN_PMU_PWR) fica
     * flutuando e ruido pode disparar wake-ups espurios. */
    rtc_gpio_init(BOARD_PIN_PMU_PWR);
    rtc_gpio_set_direction(BOARD_PIN_PMU_PWR, RTC_GPIO_MODE_INPUT_ONLY);
    rtc_gpio_pullup_en(BOARD_PIN_PMU_PWR);
    rtc_gpio_pulldown_dis(BOARD_PIN_PMU_PWR);

    esp_err_t err = esp_sleep_enable_ext0_wakeup(BOARD_PIN_PMU_PWR, 0);
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
        if (gpio_get_level(BOARD_PIN_PMU_PWR) == 0) {
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
