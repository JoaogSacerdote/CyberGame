#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

#define DISPLAY_HAL_WIDTH               480
#define DISPLAY_HAL_HEIGHT              320
#define DISPLAY_HAL_BYTES_PER_PIXEL     2       /* RGB565 */
#define DISPLAY_HAL_FRAMEBUFFER_BYTES   (DISPLAY_HAL_WIDTH * DISPLAY_HAL_HEIGHT * DISPLAY_HAL_BYTES_PER_PIXEL)

typedef void (*display_hal_trans_done_cb_t)(void *user_ctx);

esp_err_t display_hal_init(void);
esp_err_t display_hal_deinit(void);

esp_err_t display_hal_draw_bitmap(int x_start, int y_start,
                                  int x_end,   int y_end,
                                  const void *pixel_data);

esp_err_t display_hal_register_trans_done_cb(display_hal_trans_done_cb_t cb, void *user_ctx);

esp_err_t display_hal_set_backlight_percent(uint8_t pct);
esp_err_t display_hal_on(bool on);

uint16_t  display_hal_get_width(void);
uint16_t  display_hal_get_height(void);
uint8_t   display_hal_get_bytes_per_pixel(void);

void     *display_hal_get_panel_handle(void);

#ifdef __cplusplus
}
#endif
