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
#include "storage_hal.h"
#include "display_hal.h"
#include "hal_bridge.h"
#include "ui_debug.h"
#include "asset_store.h"
#include "recovery.h"

static const char *TAG = "APP_MAIN";

static void button_logger_task(void *pv)
{
    (void)pv;
    static const char *names[BTN_MAX_COUNT] = { "A", "B", "X", "Y", "START" };
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
            ui_debug_set_nfc_card(&card);
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
            ui_debug_set_nfc_scanning(true);
        } else if (!now && was_pressed) {
            ESP_LOGI(TAG, "[TEST] BTN_A solto -> NFC scanning OFF");
            nfc_hal_stop_scanning();
            ui_debug_set_nfc_scanning(false);
        }
        was_pressed = now;
        vTaskDelay(pdMS_TO_TICKS(50));
    }
}

void app_main(void)
{
    ESP_ERROR_CHECK(pmu_init());

    ESP_LOGI(TAG, "Avaliando Power Latch...");
    const pmu_boot_mode_t mode = pmu_check_boot_mode();

    if (mode == PMU_BOOT_ABORT) {
        ESP_LOGW(TAG, "Botao solto prematuramente. Voltando ao Deep Sleep.");
        pmu_enter_deep_sleep();
    }

    /* Monitor de PWR para shutdown vale tanto em jogo quanto em recovery. */
    xTaskCreate(pmu_shutdown_monitor_task, "pmu_monitor", 3072, NULL, 5, NULL);

    if (mode == PMU_BOOT_RECOVERY) {
        ESP_LOGI(TAG, "Boot RECOVERY confirmado (PWR+REC). Modo gravador.");

        if (storage_hal_init() != ESP_OK) {
            ESP_LOGE(TAG, "storage_hal_init falhou — sem como rodar testes do NAND");
        } else {
            if (storage_hal_test_write_cycle() == ESP_OK) {
                ESP_LOGI(TAG, "POST do NAND PASSOU.");
            } else {
                ESP_LOGE(TAG, "POST do NAND FALHOU — ver logs acima.");
            }
            ESP_LOGI(TAG, "---");
            storage_hal_run_full_validation();
        }

        if (recovery_init() == ESP_OK) {
            ESP_LOGI(TAG, "Recovery USB CDC ativo. Use PC para enviar comandos. Segure PWR para desligar.");
            recovery_run();   /* nunca retorna */
        } else {
            ESP_LOGE(TAG, "Falha ao iniciar USB CDC — caindo em idle. Segure PWR para desligar.");
            while (1) {
                vTaskDelay(pdMS_TO_TICKS(1000));
            }
        }
    }

    /* PMU_BOOT_NORMAL daqui em diante. */
    ESP_LOGI(TAG, "Boot NORMAL confirmado! Iniciando sistema...");

    ESP_ERROR_CHECK(button_hal_init());
    xTaskCreate(button_logger_task, "btn_logger", 3072, NULL, 4, NULL);

    ESP_ERROR_CHECK(joystick_hal_init());
    xTaskCreate(joystick_logger_task, "joy_logger", 3072, NULL, 4, NULL);

    ESP_ERROR_CHECK(nfc_hal_init());
    xTaskCreate(nfc_logger_task,       "nfc_logger", 3072, NULL, 4, NULL);
    xTaskCreate(nfc_test_trigger_task, "nfc_trig",   2560, NULL, 4, NULL);

    /* Display antes do storage: ele eh o dono do SPI2 (precisa de max_transfer_sz
     * grande para framebuffer). Storage anexa depois via spi_bus_add_device,
     * tolerando ESP_ERR_INVALID_STATE no spi_bus_initialize dele. */
    if (display_hal_init() != ESP_OK) {
        ESP_LOGE(TAG, "display_hal_init falhou — display fora. Boot continua.");
    } else if (hal_bridge_init() != ESP_OK) {
        ESP_LOGE(TAG, "hal_bridge_init falhou — sem UI. Boot continua.");
    } else if (ui_debug_init() != ESP_OK) {
        ESP_LOGE(TAG, "ui_debug_init falhou — UI sem tela de debug. Boot continua.");
        display_hal_set_backlight_percent(70);
    } else {
        display_hal_set_backlight_percent(70);
    }

    /* Storage: nao usa ESP_ERROR_CHECK — se a NAND nao responder, queremos
     * que o resto do sistema continue funcional para diagnostico via UART. */
    if (storage_hal_init() != ESP_OK) {
        ESP_LOGE(TAG, "storage_hal_init falhou — NAND inacessivel. Boot continua.");
    } else if (asset_store_init() != ESP_OK) {
        ESP_LOGE(TAG, "asset_store_init falhou — assets indisponiveis. Boot continua.");
    } else {
        size_t n = 0;
        asset_store_count(&n);
        ESP_LOGI(TAG, "asset_store pronto: %u entries", (unsigned)n);
    }

    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
