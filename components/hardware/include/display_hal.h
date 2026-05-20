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

/* === Calibracao do painel ST7796 deste console ===
 *
 * Os LEDs vermelhos deste painel especifico tem eficiencia luminica menor
 * que verdes/azuis (R aparece ~3x mais escuro que devia). O hal_bridge
 * aplica este multiplicador no canal R do RGB565 antes do byte swap,
 * com clamp em 5 bits — cores ja saturadas (R=31) ficam inalteradas.
 *
 * Se o painel for trocado por outro lote, recalibrar visualmente
 * (testar com bandeira nacional / palette colorida). Setar 1 desliga. */
#define DISPLAY_HAL_R_BOOST_MULT        2u

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
