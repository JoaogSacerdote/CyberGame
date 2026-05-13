#include <stdint.h>
#include <stdio.h>
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
#include "asset_store.h"
#include "recovery.h"
#include "ui_debug.h"     /* modo dev — acessivel pelo combo Y+START */
#include "engine.h"       /* gameplay loop */

static const char *TAG = "APP_MAIN";

#define DEV_COMBO_POLL_MS   100
#define DEV_COMBO_HOLD_MS   2000

/* Combo dev: Y + START ja pressionados ao entrar em detect_dev_combo
 * e segurados continuamente por DEV_COMBO_HOLD_MS -> entra em ui_debug.
 * Caso contrario retorna imediatamente.
 *
 * Detecta assim que main.c termina os inits — o usuario pode comecar a
 * segurar o combo durante o splash anterior do PMU e ainda pegar. */
static bool detect_dev_combo(void)
{
    if (button_hal_peek(BTN_Y) != BTN_PRESSED ||
        button_hal_peek(BTN_START) != BTN_PRESSED) {
        return false;
    }
    ESP_LOGI(TAG, "Combo dev Y+START detectado. Segure por %d ms para confirmar.",
             DEV_COMBO_HOLD_MS);
    for (int waited = 0; waited < DEV_COMBO_HOLD_MS; waited += DEV_COMBO_POLL_MS) {
        if (button_hal_peek(BTN_Y) != BTN_PRESSED ||
            button_hal_peek(BTN_START) != BTN_PRESSED) {
            ESP_LOGI(TAG, "Combo desfeito antes dos %d ms. Seguindo boot normal.",
                     DEV_COMBO_HOLD_MS);
            return false;
        }
        vTaskDelay(pdMS_TO_TICKS(DEV_COMBO_POLL_MS));
    }
    return true;
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

    /* HALs de input — button_hal claim do GPIO 3 (REC -> START) com settle interno. */
    ESP_ERROR_CHECK(button_hal_init());
    ESP_ERROR_CHECK(joystick_hal_init());
    ESP_ERROR_CHECK(nfc_hal_init());

    /* Display antes do storage: ele eh o dono do SPI2 (precisa de max_transfer_sz
     * grande para framebuffer). Storage anexa depois via spi_bus_add_device,
     * tolerando ESP_ERR_INVALID_STATE no spi_bus_initialize dele. */
    if (display_hal_init() != ESP_OK) {
        ESP_LOGE(TAG, "display_hal_init falhou — sem video. Idle.");
        while (1) vTaskDelay(pdMS_TO_TICKS(1000));
    }
    if (hal_bridge_init() != ESP_OK) {
        ESP_LOGE(TAG, "hal_bridge_init falhou — sem UI. Idle.");
        while (1) vTaskDelay(pdMS_TO_TICKS(1000));
    }
    display_hal_set_backlight_percent(70);

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

    /* Combo dev: Y+START segurados juntos por 2s -> ui_debug.
     * Caso contrario, inicia o engine normalmente. */
    if (detect_dev_combo()) {
        ESP_LOGW(TAG, "MODO DEV: iniciando ui_debug ao inves do engine.");
        if (ui_debug_init() != ESP_OK) {
            ESP_LOGE(TAG, "ui_debug_init falhou — boot continua em idle.");
        }
        while (1) vTaskDelay(pdMS_TO_TICKS(1000));
    }

    /* MODO GAME normal. */
    if (engine_init() != ESP_OK) {
        ESP_LOGE(TAG, "engine_init falhou — boot continua em idle.");
        while (1) vTaskDelay(pdMS_TO_TICKS(1000));
    }
    if (engine_start() != ESP_OK) {
        ESP_LOGE(TAG, "engine_start falhou — engine nao recebera eventos.");
    }

    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
