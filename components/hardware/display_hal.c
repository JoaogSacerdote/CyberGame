#include "display_hal.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "driver/ledc.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_st7796.h"
#include "esp_check.h"
#include "esp_log.h"

#define DISP_PIN_MOSI       7
#define DISP_PIN_SCK        8
#define DISP_PIN_CS         17
#define DISP_PIN_DC         18
#define DISP_PIN_RST        21
#define DISP_PIN_BL         38
#define DISP_PIN_PWR_EN     42      /* NPN (driver de gate): 1 = LIGA VCC, 0 = CORTA VCC */

#define DISP_PWR_STABILIZE_MS   50

#define DISP_SPI_HOST       SPI2_HOST
#define DISP_SPI_HZ         (40 * 1000 * 1000) 
#define DISP_CMD_BITS       8
#define DISP_PARAM_BITS     8

#define DISP_BL_LEDC_TIMER      LEDC_TIMER_0
#define DISP_BL_LEDC_MODE       LEDC_LOW_SPEED_MODE
#define DISP_BL_LEDC_CHANNEL    LEDC_CHANNEL_0
#define DISP_BL_LEDC_DUTY_RES   LEDC_TIMER_12_BIT
#define DISP_BL_LEDC_FREQ_HZ    10000
#define DISP_BL_DUTY_MAX        ((1U << 12) - 1U)

static const char *TAG = "DISPLAY_HAL";

static esp_lcd_panel_io_handle_t s_panel_io  = NULL;
static esp_lcd_panel_handle_t    s_panel     = NULL;
static bool                      s_inited    = false;

static display_hal_trans_done_cb_t s_user_cb       = NULL;
static void                       *s_user_cb_ctx   = NULL;

static bool IRAM_ATTR on_color_trans_done(esp_lcd_panel_io_handle_t io,
                                          esp_lcd_panel_io_event_data_t *edata,
                                          void *user_ctx)
{
    (void)io;
    (void)edata;
    (void)user_ctx;
    if (s_user_cb) {
        s_user_cb(s_user_cb_ctx);
    }
    return false;
}

static void display_hal_release_pin_holds(void)
{
    /* Se o boot vier de Deep Sleep, esses pinos podem estar travados via gpio_hold_en
       feito no display_hal_sleep(). Liberar e best-effort: falha aqui nao deve abortar
       o boot do display. */
    static const gpio_num_t pins[] = {
        DISP_PIN_PWR_EN, DISP_PIN_BL, DISP_PIN_MOSI, DISP_PIN_SCK,
        DISP_PIN_CS, DISP_PIN_DC, DISP_PIN_RST,
    };
    for (size_t i = 0; i < sizeof(pins) / sizeof(pins[0]); ++i) {
        esp_err_t err = gpio_hold_dis(pins[i]);
        if (err != ESP_OK && err != ESP_ERR_NOT_SUPPORTED) {
            ESP_LOGW(TAG, "gpio_hold_dis(%d) returned %s", pins[i], esp_err_to_name(err));
        }
    }
}

static void display_hal_power_on(void)
{
    const gpio_config_t cfg = {
        .pin_bit_mask = (1ULL << DISP_PIN_PWR_EN),
        .mode         = GPIO_MODE_OUTPUT,
        .pull_up_en   = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type    = GPIO_INTR_DISABLE,
    };
    esp_err_t err = gpio_config(&cfg);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "pwr_en gpio_config failed: %s", esp_err_to_name(err));
        return;
    }
    /** err = gpio_set_level(DISP_PIN_PWR_EN, 0);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "pwr_en set 0 failed: %s", esp_err_to_name(err));
        return;
    } **/
   err = gpio_set_level(DISP_PIN_PWR_EN, 1); // <-- Alterado para 1 (NPN LIGA com HIGH)
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "pwr_en set 1 failed: %s", esp_err_to_name(err));
        return;
    }
    vTaskDelay(pdMS_TO_TICKS(DISP_PWR_STABILIZE_MS));
}

static esp_err_t display_hal_backlight_init(void)
{
    const ledc_timer_config_t timer_cfg = {
        .speed_mode      = DISP_BL_LEDC_MODE,
        .timer_num       = DISP_BL_LEDC_TIMER,
        .duty_resolution = DISP_BL_LEDC_DUTY_RES,
        .freq_hz         = DISP_BL_LEDC_FREQ_HZ,
        .clk_cfg         = LEDC_AUTO_CLK,
    };
    ESP_RETURN_ON_ERROR(ledc_timer_config(&timer_cfg), TAG, "ledc_timer_config failed");

    const ledc_channel_config_t ch_cfg = {
        .gpio_num   = DISP_PIN_BL,
        .speed_mode = DISP_BL_LEDC_MODE,
        .channel    = DISP_BL_LEDC_CHANNEL,
        .timer_sel  = DISP_BL_LEDC_TIMER,
        .duty       = 0,
        .hpoint     = 0,
        .intr_type  = LEDC_INTR_DISABLE,
    };
    ESP_RETURN_ON_ERROR(ledc_channel_config(&ch_cfg), TAG, "ledc_channel_config failed");

    return ESP_OK;
}

static esp_err_t display_hal_spi_bus_init(void)
{
    /* MISO=9 pertence ao storage_hal (NAND W25N01GV). Display nao le, mas
     * roteamos a linha aqui para que o storage possa anexar via spi_bus_add_device
     * sem precisar reinicializar o bus. Display eh dono do bus por exigir
     * max_transfer_sz muito maior. */
    const spi_bus_config_t bus_cfg = {
        .mosi_io_num     = DISP_PIN_MOSI,
        .miso_io_num     = 9,
        .sclk_io_num     = DISP_PIN_SCK,
        .quadwp_io_num   = -1,
        .quadhd_io_num   = -1,
        .max_transfer_sz = DISPLAY_HAL_WIDTH * 80 * DISPLAY_HAL_BYTES_PER_PIXEL,
    };
    return spi_bus_initialize(DISP_SPI_HOST, &bus_cfg, SPI_DMA_CH_AUTO);
}

esp_err_t display_hal_init(void)
{
    if (s_inited) {
        return ESP_OK;
    }

    display_hal_release_pin_holds();
    display_hal_power_on();

    ESP_RETURN_ON_ERROR(display_hal_backlight_init(), TAG, "backlight init failed");
    ESP_RETURN_ON_ERROR(display_hal_spi_bus_init(),   TAG, "spi bus init failed");

    const esp_lcd_panel_io_spi_config_t io_cfg = {
        .cs_gpio_num         = DISP_PIN_CS,
        .dc_gpio_num         = DISP_PIN_DC,
        .spi_mode            = 0,
        .pclk_hz             = DISP_SPI_HZ,
        .trans_queue_depth   = 10,
        .lcd_cmd_bits        = DISP_CMD_BITS,
        .lcd_param_bits      = DISP_PARAM_BITS,
        .on_color_trans_done = on_color_trans_done,
        .user_ctx            = NULL,
    };
    ESP_RETURN_ON_ERROR(
        esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)DISP_SPI_HOST, &io_cfg, &s_panel_io),
        TAG, "panel_io_spi failed");

    const esp_lcd_panel_dev_config_t panel_cfg = {
        .reset_gpio_num = DISP_PIN_RST,
        /* Calibracao empirica 2026-05-13: confirmado por 24-cell color grid
         * que o display espera BGR (vermelho e azul trocados em modo RGB).
         * Combinado com byte_swap_inplace no hal_bridge, fecha o pipeline. */
        .rgb_ele_order  = LCD_RGB_ELEMENT_ORDER_BGR,
        .bits_per_pixel = 16,
    };
    
    ESP_RETURN_ON_ERROR(
        esp_lcd_new_panel_st7796(s_panel_io, &panel_cfg, &s_panel),
        TAG, "panel_st7796 failed");

    ESP_RETURN_ON_ERROR(esp_lcd_panel_reset(s_panel),                     TAG, "panel reset");
    ESP_RETURN_ON_ERROR(esp_lcd_panel_init(s_panel),                      TAG, "panel init");
    ESP_RETURN_ON_ERROR(esp_lcd_panel_invert_color(s_panel, false),       TAG, "invert color");
    ESP_RETURN_ON_ERROR(esp_lcd_panel_swap_xy(s_panel, true),             TAG, "swap xy");
    ESP_RETURN_ON_ERROR(esp_lcd_panel_mirror(s_panel, true, true),       TAG, "mirror");
    ESP_RETURN_ON_ERROR(esp_lcd_panel_disp_on_off(s_panel, true),         TAG, "disp on");

    s_inited = true;
    ESP_LOGI(TAG, "display_hal initialized at %dx%d, %d bpp",
             DISPLAY_HAL_WIDTH, DISPLAY_HAL_HEIGHT, DISPLAY_HAL_BYTES_PER_PIXEL * 8);
    return ESP_OK;
}

esp_err_t display_hal_deinit(void)
{
    if (!s_inited) {
        return ESP_OK;
    }
    if (s_panel) {
        esp_lcd_panel_del(s_panel);
        s_panel = NULL;
    }
    if (s_panel_io) {
        esp_lcd_panel_io_del(s_panel_io);
        s_panel_io = NULL;
    }
    spi_bus_free(DISP_SPI_HOST);
    s_inited = false;
    return ESP_OK;
}

esp_err_t display_hal_draw_bitmap(int x_start, int y_start,
                                  int x_end,   int y_end,
                                  const void *pixel_data)
{
    if (!s_inited || !s_panel) {
        return ESP_ERR_INVALID_STATE;
    }
    return esp_lcd_panel_draw_bitmap(s_panel, x_start, y_start, x_end, y_end, pixel_data);
}

esp_err_t display_hal_register_trans_done_cb(display_hal_trans_done_cb_t cb, void *user_ctx)
{
    s_user_cb     = cb;
    s_user_cb_ctx = user_ctx;
    return ESP_OK;
}

esp_err_t display_hal_set_backlight_percent(uint8_t pct)
{
    if (pct > 100) pct = 100;
    const uint32_t duty = ((uint32_t)pct * DISP_BL_DUTY_MAX) / 100U;
    ESP_RETURN_ON_ERROR(ledc_set_duty(DISP_BL_LEDC_MODE, DISP_BL_LEDC_CHANNEL, duty),
                        TAG, "ledc_set_duty");
    return ledc_update_duty(DISP_BL_LEDC_MODE, DISP_BL_LEDC_CHANNEL);
}

esp_err_t display_hal_on(bool on)
{
    if (!s_inited || !s_panel) {
        return ESP_ERR_INVALID_STATE;
    }
    return esp_lcd_panel_disp_on_off(s_panel, on);
}

uint16_t display_hal_get_width(void)              { return DISPLAY_HAL_WIDTH; }
uint16_t display_hal_get_height(void)             { return DISPLAY_HAL_HEIGHT; }
uint8_t  display_hal_get_bytes_per_pixel(void)    { return DISPLAY_HAL_BYTES_PER_PIXEL; }
void    *display_hal_get_panel_handle(void)       { return (void *)s_panel; }
