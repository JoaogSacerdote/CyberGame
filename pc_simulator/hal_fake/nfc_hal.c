#include "nfc_hal.h"
#include <string.h>

esp_err_t nfc_hal_init(void)           { return ESP_OK; }
esp_err_t nfc_hal_start_scanning(void) { return ESP_OK; }
esp_err_t nfc_hal_stop_scanning(void)  { return ESP_OK; }

bool nfc_hal_wait_card(nfc_card_t *card, uint32_t timeout_ms) {
    (void)card; (void)timeout_ms;
    return false;
}
