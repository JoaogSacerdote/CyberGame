#include <stdint.h>
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_idf_version.h"
#include "esp_log.h"
#include "esp_system.h"

#include "version.h"
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

static const char *reset_reason_name(esp_reset_reason_t r)
{
    switch (r) {
        case ESP_RST_POWERON:    return "POWERON";
        case ESP_RST_EXT:        return "EXT";
        case ESP_RST_SW:         return "SW";
        case ESP_RST_PANIC:      return "PANIC";
        case ESP_RST_INT_WDT:    return "INT_WDT";
        case ESP_RST_TASK_WDT:   return "TASK_WDT";
        case ESP_RST_WDT:        return "OTHER_WDT";
        case ESP_RST_DEEPSLEEP:  return "DEEPSLEEP_WAKE";
        case ESP_RST_BROWNOUT:   return "BROWNOUT";
        case ESP_RST_SDIO:       return "SDIO";
        default:                 return "UNKNOWN";
    }
}

static void log_boot_banner(void)
{
    const esp_reset_reason_t r = esp_reset_reason();
    ESP_LOGI(TAG, "======================================================");
    ESP_LOGI(TAG, "  CyberGame v" CYBERGAME_VERSION_STR " (idf %s)", IDF_VER);
    ESP_LOGI(TAG, "  reset reason: %s", reset_reason_name(r));
    ESP_LOGI(TAG, "======================================================");
}

void app_main(void)
{
    log_boot_banner();

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
            /* A validacao fisica completa (destrutiva, ~5-10 s) NAO roda mais
             * automaticamente: viraria risco a cada entrada em recovery, que
             * agora tambem e o modo de upload de assets. Disponivel sob
             * demanda pelo comando CMD_SELFTEST do protocolo de recovery. */
        }

        /* asset_store sobe sobre o storage_hal — necessario para os comandos
         * PUT/GET/LIST do recovery. Se falhar, o recovery ainda roda (PING),
         * mas comandos de asset retornam NACK. */
        if (asset_store_init() != ESP_OK) {
            ESP_LOGE(TAG, "asset_store_init falhou — comandos de asset indisponiveis.");
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

    /* ============================================================
     * === MODO BRING-UP TEMPORARIO (2026-05-26) — INICIO ===
     * Todos os HALs de hardware aqui tolerantes a falha: log + segue.
     * Para reverter: ver CyberGameCore/CHANGELOG/entries/2026-05-26T1945-bring-up-mode-completo.md
     * Bloco original preservado integralmente nessa entrada.
     * ============================================================ */

    if (button_hal_init() != ESP_OK) {
        ESP_LOGW(TAG, "[BRING-UP] buttons ausentes — boot continua sem input de botoes.");
    }
    if (joystick_hal_init() != ESP_OK) {
        ESP_LOGW(TAG, "[BRING-UP] joystick ausente — boot continua sem ADC do analog.");
    }
    if (nfc_hal_init() != ESP_OK) {
        ESP_LOGW(TAG, "[BRING-UP] NFC ausente — boot continua sem leitura de cartao.");
    }

    /* Display: sem ele nao tem como rodar LVGL/engine/ui_debug. Marca flag. */
    bool ui_ok = true;
    if (display_hal_init() != ESP_OK) {
        ESP_LOGW(TAG, "[BRING-UP] display ausente — boot continua mas sem video.");
        ui_ok = false;
    } else if (hal_bridge_init() != ESP_OK) {
        ESP_LOGW(TAG, "[BRING-UP] hal_bridge falhou — boot continua mas sem UI.");
        ui_ok = false;
    } else {
        display_hal_set_backlight_percent(70);
    }

    /* Storage: se a NAND nao responder, segue (ja era tolerante). */
    if (storage_hal_init() != ESP_OK) {
        ESP_LOGW(TAG, "[BRING-UP] NAND ausente — boot continua sem asset_store.");
    } else if (asset_store_init() != ESP_OK) {
        ESP_LOGW(TAG, "[BRING-UP] asset_store_init falhou — boot continua sem assets.");
    } else {
        size_t n = 0;
        asset_store_count(&n);
        ESP_LOGI(TAG, "asset_store pronto: %u entries", (unsigned)n);
    }

    /* Sem UI nao tem como rodar engine nem ui_debug (ambos chamam LVGL).
     * Idle com heartbeat de 5s pra deixar UART respirar e mostrar que vive. */
    if (!ui_ok) {
        ESP_LOGW(TAG, "[BRING-UP] UI indisponivel — pulando engine/ui_debug. Heartbeat a cada 5s.");
        while (1) {
            ESP_LOGW(TAG, "[BRING-UP] alive — sem UI");
            vTaskDelay(pdMS_TO_TICKS(5000));
        }
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
    /* === MODO BRING-UP TEMPORARIO (2026-05-26) — FIM === */

    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
