#include "ws2812_hal.h"

/* Stubs — os LEDs WS2812 nao existem no simulador. */

esp_err_t ws2812_hal_init(void)                               { return ESP_OK; }
esp_err_t ws2812_hal_set_pixel(uint8_t i, uint8_t r, uint8_t g, uint8_t b) {
    (void)i; (void)r; (void)g; (void)b; return ESP_OK;
}
esp_err_t ws2812_hal_set_all(uint8_t r, uint8_t g, uint8_t b) {
    (void)r; (void)g; (void)b; return ESP_OK;
}
esp_err_t ws2812_hal_clear(void)                              { return ESP_OK; }
esp_err_t ws2812_hal_refresh(void)                            { return ESP_OK; }
