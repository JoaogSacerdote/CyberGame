#include "hal_bridge.h"

#include <stddef.h>
#include <stdint.h>

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

/* Calibracao 2026-05-13: RGB565 e armazenado como uint16_t little-endian
 * na memoria do ESP32, mas o ST7796 espera os bytes em ordem big-endian
 * via SPI. Combinado com BGR mode no display_hal, fecha o pipeline.
 *
 * Adicionalmente, os LEDs vermelhos deste painel ST7796 tem eficiencia
 * luminica menor que verdes/azuis (R aparece ~3x mais escuro que devia).
 * Aplicamos boost 2x no canal R com clamp em 5 bits antes do byte swap.
 * Cores saturadas (R=31) ficam inalteradas. */
#define HAL_BRIDGE_R_BOOST_MULT  2u

static inline void rb_boost_and_byte_swap_inplace(uint16_t *pixels, size_t count)
{
    for (size_t i = 0; i < count; ++i) {
        uint16_t p = pixels[i];

        /* Boost canal R (bits 15-11 do pixel em RGB565). Empiricamente
         * confirmado: esses bits geram o canal R visual no display em
         * BGR mode + byte swap. */
        uint16_t r = (p >> 11) & 0x1Fu;
        uint16_t boosted = r * HAL_BRIDGE_R_BOOST_MULT;
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
    const size_t pixels = (size_t)(x2 - x1 + 1) * (size_t)(y2 - y1 + 1);

    rb_boost_and_byte_swap_inplace((uint16_t *)px_map, pixels);

    /* display_hal espera (x_end, y_end) exclusivos -> +1. */
    const esp_err_t err = display_hal_draw_bitmap(x1, y1, x2 + 1, y2 + 1, px_map);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "display_hal_draw_bitmap retornou %s", esp_err_to_name(err));
        /* Sinaliza ready imediatamente para nao travar o pipeline do LVGL. */
        lv_display_flush_ready(disp);
    }
    /* Em sucesso, lv_display_flush_ready sera chamado pelo
     * display_trans_done_cb quando o DMA acabar. */
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
