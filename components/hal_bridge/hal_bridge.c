#include "hal_bridge.h"

#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_attr.h"
#include "esp_check.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_timer.h"

#include "display_hal.h"

static const char *TAG = "HAL_BRIDGE";

#define LV_TICK_PERIOD_MS       5
/* Delay maximo entre chamadas do lv_timer_handler. lv_timer_handler retorna
 * o tempo ate o proximo evento; aplicamos clamp com minimo de uma tick do
 * FreeRTOS (vTaskDelay(0) nao dorme — busy loop dispararia o WDT). */
#define LV_TASK_PERIOD_MAX_MS   100

/* Strip de 32 linhas. Mesmo tamanho que o smoke test validou: cabe folgado
 * abaixo do max_transfer_sz do display_hal (480*80*2 = 76 800) e cabe na
 * RAM interna DMA (~30 KB). Usamos dois buffers para double buffering. */
#define HAL_BRIDGE_STRIP_LINES  32
#define HAL_BRIDGE_STRIP_PIXELS (DISPLAY_HAL_WIDTH * HAL_BRIDGE_STRIP_LINES)
#define HAL_BRIDGE_STRIP_BYTES  (HAL_BRIDGE_STRIP_PIXELS * DISPLAY_HAL_BYTES_PER_PIXEL)

static lv_display_t     *s_lv_disp        = NULL;
static esp_timer_handle_t s_lv_tick_timer = NULL;
static void              *s_buf1          = NULL;
static void              *s_buf2          = NULL;

/* Pipeline de pixel pro ST7796: RGB565 little-endian em memoria, byte swap
 * pra big-endian via SPI + BGR mode do display_hal + boost de canal R
 * (DISPLAY_HAL_R_BOOST_MULT — calibracao do painel, ver display_hal.h). */

static inline void rb_boost_and_byte_swap_inplace(uint16_t *pixels, size_t count)
{
    for (size_t i = 0; i < count; ++i) {
        uint16_t p = pixels[i];

        /* Boost canal R (bits 15-11 do pixel em RGB565). */
        uint16_t r = (p >> 11) & 0x1Fu;
        uint16_t boosted = r * DISPLAY_HAL_R_BOOST_MULT;
        if (boosted > 31u) boosted = 31u;
        p = (uint16_t)((p & 0x07FFu) | (boosted << 11));

        /* Byte swap LE -> BE para o ST7796 ler corretamente via SPI. */
        pixels[i] = (uint16_t)((p << 8) | (p >> 8));
    }
}

/* Chamado pelo display_hal quando o DMA do flush terminou. Sinaliza ao LVGL
 * que pode reusar o buffer. ISR-safe. */
static void IRAM_ATTR display_trans_done_cb(void *user_ctx)
{
    lv_display_t *disp = (lv_display_t *)user_ctx;
    if (disp) {
        lv_display_flush_ready(disp);
    }
}

static void flush_cb(lv_display_t *disp, const lv_area_t *area, uint8_t *px_map)
{
    const int x1 = area->x1;
    const int y1 = area->y1;
    const int x2 = area->x2;     /* coordenadas do LVGL sao inclusivas    */
    const int y2 = area->y2;
    const int width = x2 - x1 + 1;
    const int height = y2 - y1 + 1;
    const size_t line_bytes = (size_t)width * DISPLAY_HAL_BYTES_PER_PIXEL;

    /* Detecta stride: LVGL pode alinhar linhas e introduzir padding.
     * Se o draw buf ativo tem stride > line_bytes, copiamos linha-a-linha
     * para buffer denso antes do display_hal_draw_bitmap. */
    uint8_t *src_buf = px_map;
    uint8_t *temp_buf = NULL;

    {
        const lv_draw_buf_t *buf_active = lv_display_get_buf_active(disp);
        if (buf_active && buf_active->header.stride > line_bytes) {
            /* Buffer tem stride > width*bpp. Copia linha-a-linha densamente. */
            size_t stride_bytes = (size_t)buf_active->header.stride;
            temp_buf = (uint8_t *)malloc(line_bytes * height);
            if (temp_buf) {
                for (int y = 0; y < height; y++) {
                    const uint8_t *src_line = px_map + (y * stride_bytes);
                    uint8_t *dst_line = temp_buf + (y * line_bytes);
                    memcpy(dst_line, src_line, line_bytes);
                }
                src_buf = temp_buf;
            }
        }
    }

    rb_boost_and_byte_swap_inplace((uint16_t *)src_buf, (size_t)width * height);

    /* display_hal espera (x_end, y_end) exclusivos -> +1. */
    const esp_err_t err = display_hal_draw_bitmap(x1, y1, x2 + 1, y2 + 1, src_buf);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "display_hal_draw_bitmap retornou %s", esp_err_to_name(err));
        /* Sinaliza ready imediatamente para nao travar o pipeline do LVGL. */
        lv_display_flush_ready(disp);
    }
    /* Em sucesso, lv_display_flush_ready sera chamado pelo
     * display_trans_done_cb quando o DMA acabar. */

    if (temp_buf) {
        free(temp_buf);
    }
}

static void IRAM_ATTR lv_tick_timer_cb(void *arg)
{
    (void)arg;
    lv_tick_inc(LV_TICK_PERIOD_MS);
}

static void lv_task(void *arg)
{
    (void)arg;
    while (1) {
        lv_lock();
        uint32_t delay_ms = lv_timer_handler();
        lv_unlock();

        if (delay_ms > LV_TASK_PERIOD_MAX_MS) {
            delay_ms = LV_TASK_PERIOD_MAX_MS;
        }
        if (delay_ms < (uint32_t)portTICK_PERIOD_MS) {
            delay_ms = (uint32_t)portTICK_PERIOD_MS;
        }
        vTaskDelay(pdMS_TO_TICKS(delay_ms));
    }
}

esp_err_t hal_bridge_init(void)
{
    if (s_lv_disp) {
        return ESP_OK;
    }

    lv_init();

    s_lv_disp = lv_display_create(DISPLAY_HAL_WIDTH, DISPLAY_HAL_HEIGHT);
    if (!s_lv_disp) {
        ESP_LOGE(TAG, "lv_display_create falhou");
        return ESP_ERR_NO_MEM;
    }

    /* Buffers em RAM interna DMA. ~30 KB cada -> 60 KB total, cabe folgado
     * nos ~314 KB de DRAM disponivel. Evita o caminho de "priv DMA buffer"
     * do spi_master que falhou no smoke test com PSRAM grande. */
    s_buf1 = heap_caps_malloc(HAL_BRIDGE_STRIP_BYTES, MALLOC_CAP_DMA);
    s_buf2 = heap_caps_malloc(HAL_BRIDGE_STRIP_BYTES, MALLOC_CAP_DMA);
    if (!s_buf1 || !s_buf2) {
        ESP_LOGE(TAG, "alloc buffers DMA falhou (%u bytes cada)",
                 (unsigned)HAL_BRIDGE_STRIP_BYTES);
        return ESP_ERR_NO_MEM;
    }

    lv_display_set_color_format(s_lv_disp, LV_COLOR_FORMAT_RGB565);
    lv_display_set_buffers(s_lv_disp, s_buf1, s_buf2, HAL_BRIDGE_STRIP_BYTES,
                           LV_DISPLAY_RENDER_MODE_PARTIAL);
    lv_display_set_flush_cb(s_lv_disp, flush_cb);

    /* Liga a sincronizacao DMA -> LVGL. */
    display_hal_register_trans_done_cb(display_trans_done_cb, s_lv_disp);

    /* Tick periodico de 5 ms. */
    const esp_timer_create_args_t tick_args = {
        .callback        = lv_tick_timer_cb,
        .dispatch_method = ESP_TIMER_TASK,
        .name            = "lv_tick",
    };
    ESP_RETURN_ON_ERROR(esp_timer_create(&tick_args, &s_lv_tick_timer),
                        TAG, "esp_timer_create");
    ESP_RETURN_ON_ERROR(
        esp_timer_start_periodic(s_lv_tick_timer, LV_TICK_PERIOD_MS * 1000ULL),
        TAG, "esp_timer_start_periodic");

    /* Task que toca o motor do LVGL. Stack 6 KB e prioridade 4, mesma dos
     * loggers. Pode subir se houver frame skip futuramente. */
    if (xTaskCreate(lv_task, "lvgl", 6144, NULL, 4, NULL) != pdPASS) {
        ESP_LOGE(TAG, "xTaskCreate(lvgl) falhou");
        return ESP_ERR_NO_MEM;
    }

    ESP_LOGI(TAG, "hal_bridge ok (LVGL %d.%d.%d, %dx%d, RGB565, partial 32 lines x2)",
             LVGL_VERSION_MAJOR, LVGL_VERSION_MINOR, LVGL_VERSION_PATCH,
             DISPLAY_HAL_WIDTH, DISPLAY_HAL_HEIGHT);
    return ESP_OK;
}

lv_display_t *hal_bridge_get_display(void)
{
    return s_lv_disp;
}
