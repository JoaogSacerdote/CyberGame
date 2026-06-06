#include "ws2812_hal.h"

#include <assert.h>

#include "board_pins.h"
#include "led_strip.h"
#include "esp_log.h"

static const char *TAG = "WS2812_HAL";

/* RMT a 10 MHz: resolucao de 100 ns/tick, suficiente para os tempos do
 * protocolo WS2812 (T0H/T1H na casa de centenas de ns). Sem DMA: sao so
 * 3 LEDs, o buffer RMT padrao da conta. */
#define WS2812_RMT_RESOLUTION_HZ   (10 * 1000 * 1000)
#define WS2812_RMT_MEM_SYMBOLS     64

static led_strip_handle_t s_strip = NULL;

esp_err_t ws2812_hal_init(void)
{
    if (s_strip != NULL) {
        return ESP_OK;   /* idempotente */
    }

    const led_strip_config_t strip_cfg = {
        .strip_gpio_num         = BOARD_PIN_WS2812_DATA,
        .max_leds               = WS2812_LED_COUNT,
        .led_model              = LED_MODEL_WS2812,
        .color_component_format = LED_STRIP_COLOR_COMPONENT_FMT_GRB,
        .flags = {
            .invert_out = false,
        },
    };

    const led_strip_rmt_config_t rmt_cfg = {
        .clk_src           = RMT_CLK_SRC_DEFAULT,
        .resolution_hz     = WS2812_RMT_RESOLUTION_HZ,
        .mem_block_symbols = WS2812_RMT_MEM_SYMBOLS,
        .flags = {
            .with_dma = false,
        },
    };

    const esp_err_t err = led_strip_new_rmt_device(&strip_cfg, &rmt_cfg, &s_strip);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "led_strip_new_rmt_device falhou: %s", esp_err_to_name(err));
        s_strip = NULL;
        return err;
    }

    led_strip_clear(s_strip);   /* garante todos apagados no boot */
    ESP_LOGI(TAG, "WS2812 pronto: %d LEDs no GPIO %d", WS2812_LED_COUNT, BOARD_PIN_WS2812_DATA);
    return ESP_OK;
}

esp_err_t ws2812_hal_set_pixel(uint8_t idx, uint8_t r, uint8_t g, uint8_t b)
{
    assert(idx < WS2812_LED_COUNT);
    if (s_strip == NULL) {
        return ESP_ERR_INVALID_STATE;
    }
    return led_strip_set_pixel(s_strip, idx, r, g, b);
}

esp_err_t ws2812_hal_set_all(uint8_t r, uint8_t g, uint8_t b)
{
    if (s_strip == NULL) {
        return ESP_ERR_INVALID_STATE;
    }
    for (uint8_t i = 0; i < WS2812_LED_COUNT; i++) {
        const esp_err_t err = led_strip_set_pixel(s_strip, i, r, g, b);
        if (err != ESP_OK) {
            return err;
        }
    }
    return ESP_OK;
}

esp_err_t ws2812_hal_clear(void)
{
    if (s_strip == NULL) {
        return ESP_ERR_INVALID_STATE;
    }
    return led_strip_clear(s_strip);
}

esp_err_t ws2812_hal_refresh(void)
{
    if (s_strip == NULL) {
        return ESP_ERR_INVALID_STATE;
    }
    return led_strip_refresh(s_strip);
}
