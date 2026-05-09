#include "recovery.h"

#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_check.h"
#include "esp_log.h"
#include "tinyusb.h"
#include "tusb_cdc_acm.h"

static const char *TAG = "RECOVERY";

#define LINE_BUF_SIZE 128

static void send_response(const char *msg)
{
    tinyusb_cdcacm_write_queue(TINYUSB_CDC_ACM_0,
                               (const uint8_t *)msg, strlen(msg));
    tinyusb_cdcacm_write_flush(TINYUSB_CDC_ACM_0, pdMS_TO_TICKS(100));
}

static void process_line(const char *line)
{
    ESP_LOGI(TAG, "RX: '%s'", line);
    if (strcmp(line, "PING") == 0) {
        send_response("PONG\n");
    } else {
        send_response("UNKNOWN\n");
    }
}

esp_err_t recovery_init(void)
{
    const tinyusb_config_t tusb_cfg = {
        .device_descriptor        = NULL,
        .string_descriptor        = NULL,
        .external_phy             = false,
        .configuration_descriptor = NULL,
    };
    ESP_RETURN_ON_ERROR(tinyusb_driver_install(&tusb_cfg), TAG, "tinyusb install failed");

    const tinyusb_config_cdcacm_t acm_cfg = {
        .usb_dev                       = TINYUSB_USBDEV_0,
        .cdc_port                      = TINYUSB_CDC_ACM_0,
        .rx_unread_buf_sz              = 256,
        .callback_rx                   = NULL,
        .callback_rx_wanted_char       = NULL,
        .callback_line_state_changed   = NULL,
        .callback_line_coding_changed  = NULL,
    };
    ESP_RETURN_ON_ERROR(tusb_cdc_acm_init(&acm_cfg), TAG, "cdc init failed");

    ESP_LOGI(TAG, "USB CDC pronto — PC deve enxergar nova porta serial.");
    return ESP_OK;
}

void recovery_run(void)
{
    char    line[LINE_BUF_SIZE];
    size_t  line_len = 0;
    uint8_t rx_buf[64];

    ESP_LOGI(TAG, "Entrando em loop CDC. Aguardando comandos do PC...");

    while (1) {
        size_t rx_len = 0;
        const esp_err_t err = tinyusb_cdcacm_read(TINYUSB_CDC_ACM_0,
                                                  rx_buf, sizeof(rx_buf), &rx_len);
        if (err == ESP_OK && rx_len > 0) {
            for (size_t i = 0; i < rx_len; ++i) {
                const char c = (char)rx_buf[i];
                if (c == '\n' || c == '\r') {
                    if (line_len > 0) {
                        line[line_len] = '\0';
                        process_line(line);
                        line_len = 0;
                    }
                } else if (line_len < sizeof(line) - 1) {
                    line[line_len++] = c;
                }
            }
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}
