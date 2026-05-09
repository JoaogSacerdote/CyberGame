#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

#include "pmu.h"
#include "button_hal.h"
#include "joystick_hal.h"
#include "nfc_hal.h"

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

static void joystick_logger_task(void *pv)
{
    (void)pv;
    joystick_data_t last = { 0, 0 };
    while (1) {
        const joystick_data_t now = joystick_hal_get_state();
        if (abs(now.x - last.x) >= 10 || abs(now.y - last.y) >= 10) {
            ESP_LOGI(TAG, "Joystick: x=%4d y=%4d", now.x, now.y);
            last = now;
        }
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

static void nfc_logger_task(void *pv)
{
    (void)pv;
    nfc_card_t card;
    char hex[NFC_UID_MAX_LEN * 2 + 1];
    while (1) {
        if (nfc_hal_wait_card(&card, UINT32_MAX)) {
            for (int i = 0; i < card.uid_len; ++i) {
                sprintf(&hex[i * 2], "%02X", card.uid[i]);
            }
            hex[card.uid_len * 2] = '\0';
            ESP_LOGI(TAG, "Cartao NFC: UID=%s (len=%d, ATQA=0x%04X, SAK=0x%02X)",
                     hex, card.uid_len, card.atqa, card.sak);
        }
    }
}

/* TEMPORARIO (apenas para teste de hardware): enquanto BTN_A estiver
 * pressionado, pede ao nfc_hal para varrer. Simula a logica que game_logic
 * implementara depois — quando o jogo solicitar uma carta especifica.
 * Pode ser removido inteiro quando game_logic existir. */
static void nfc_test_trigger_task(void *pv)
{
    (void)pv;
    bool was_pressed = false;
    while (1) {
        const bool now = (button_hal_peek(BTN_A) == BTN_PRESSED);
        if (now && !was_pressed) {
            ESP_LOGI(TAG, "[TEST] BTN_A pressionado -> NFC scanning ON");
            nfc_hal_start_scanning();
        } else if (!now && was_pressed) {
            ESP_LOGI(TAG, "[TEST] BTN_A solto -> NFC scanning OFF");
            nfc_hal_stop_scanning();
        }
        was_pressed = now;
        vTaskDelay(pdMS_TO_TICKS(50));
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

    ESP_ERROR_CHECK(joystick_hal_init());
    xTaskCreate(joystick_logger_task, "joy_logger", 3072, NULL, 4, NULL);

    ESP_ERROR_CHECK(nfc_hal_init());
    xTaskCreate(nfc_logger_task,       "nfc_logger", 3072, NULL, 4, NULL);
    xTaskCreate(nfc_test_trigger_task, "nfc_trig",   2560, NULL, 4, NULL);

    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
