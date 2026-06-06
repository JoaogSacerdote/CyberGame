#include "display_hal.h"
#include "esp_log.h"
#include "driver/spi_master.h"
#include "driver/gpio.h"
#include "esp_sleep.h"
#include "rom/ets_sys.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_vendor.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_st7796.h"

// AVISO: Nenhuma menção a "lvgl.h" aqui. Arquitetura 100% Ortogonal.

static const char *TAG = "DISPLAY_HAL";

#define LCD_HOST            SPI2_HOST
#define LCD_PIXEL_CLOCK_HZ  (40 * 1000 * 1000)
#define LCD_H_RES           480
#define LCD_V_RES           320

#define PIN_NUM_MOSI        7
#define PIN_NUM_CLK         8
#define PIN_NUM_CS          17
#define PIN_NUM_DC          18

#define PIN_NUM_RST         -1 // -1 indica que não há reset físico em hardware
#define PIN_NUM_BL          38 
#define PIN_NUM_TRANSISTOR  42 // Transistor PNP para controle de energia da tela

/**
 * @brief Callback de Flush nativo via DMA.
 * Não precisa saber o que é LVGL. Ele pega coordenadas cruas e um ponteiro de cores.
 */
void display_flush_cb(void *disp_drv, int x1, int y1, int x2, int y2, uint8_t *color_p) {
    esp_lcd_panel_handle_t panel_handle = (esp_lcd_panel_handle_t) disp_drv;
    
    // O esp_lcd_panel_draw_bitmap exige que as coordenadas finais (x2 e y2) sejam exclusivas (+1)
    esp_lcd_panel_draw_bitmap(panel_handle, x1, y1, x2 + 1, y2 + 1, color_p);
}

esp_err_t display_hal_init(esp_lcd_panel_handle_t *ret_panel) {
    ESP_LOGI(TAG, "Acordando display e liberando retencao de pinos (Hold)...");

    // 1. Liberação de retenção (Hold) caso o chip esteja voltando do Deep Sleep
    gpio_hold_dis(PIN_NUM_TRANSISTOR);
    gpio_hold_dis(PIN_NUM_MOSI);
    gpio_hold_dis(PIN_NUM_CLK);
    gpio_hold_dis(PIN_NUM_CS);
    gpio_hold_dis(PIN_NUM_DC);
    gpio_hold_dis(PIN_NUM_RST);
    gpio_hold_dis(PIN_NUM_BL);

    // 2. Acionar Transístor PNP (High-Side Switch)
    gpio_config_t pnp_conf = {
        .mode = GPIO_MODE_OUTPUT,
        .pin_bit_mask = (1ULL << PIN_NUM_TRANSISTOR)
    };
    gpio_config(&pnp_conf);
    gpio_set_level(PIN_NUM_TRANSISTOR, 0); // LIGA a energia bruta da tela
    vTaskDelay(pdMS_TO_TICKS(50)); // Aguarda a estabilização da tensão de 3.3V no painel

    // 3. Configura a luz de fundo (Começa apagada)
    gpio_config_t bk_conf = {
        .mode = GPIO_MODE_OUTPUT,
        .pin_bit_mask = (1ULL << PIN_NUM_BL)
    };
    gpio_config(&bk_conf);
    gpio_set_level(PIN_NUM_BL, 0); 

    // 4. Configuração do barramento SPI (DMA)
    spi_bus_config_t buscfg = {
        .sclk_io_num = PIN_NUM_CLK,
        .mosi_io_num = PIN_NUM_MOSI,
        .miso_io_num = -1,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = (LCD_H_RES * LCD_V_RES * sizeof(uint16_t)) / 10
    };
    ESP_ERROR_CHECK(spi_bus_initialize(LCD_HOST, &buscfg, SPI_DMA_CH_AUTO));

    // 5. Configuração do Driver do Painel
    esp_lcd_panel_io_handle_t io_handle = NULL;
    esp_lcd_panel_io_spi_config_t io_config = {
        .dc_gpio_num = PIN_NUM_DC,
        .cs_gpio_num = PIN_NUM_CS,
        .pclk_hz = LCD_PIXEL_CLOCK_HZ,
        .lcd_cmd_bits = 8,
        .lcd_param_bits = 8,
        .spi_mode = 0,
        .trans_queue_depth = 10,
        .on_color_trans_done = NULL, // Removido o callback que era preso ao LVGL
    };
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)LCD_HOST, &io_config, &io_handle));

    esp_lcd_panel_handle_t panel_handle = NULL;
    esp_lcd_panel_dev_config_t panel_config = {
        .reset_gpio_num = PIN_NUM_RST,
        .rgb_ele_order = LCD_RGB_ELEMENT_ORDER_BGR, 
        .bits_per_pixel = 16,
    };
    ESP_ERROR_CHECK(esp_lcd_new_panel_st7796(io_handle, &panel_config, &panel_handle));

    ESP_ERROR_CHECK(esp_lcd_panel_reset(panel_handle));
    ESP_ERROR_CHECK(esp_lcd_panel_init(panel_handle));
    ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(panel_handle, true));

    // Liga os LEDs
    gpio_set_level(PIN_NUM_BL, 1);

    *ret_panel = panel_handle;
    ESP_LOGI(TAG, "Tela Inicializada com Sucesso! VCC, SPI e DMA ativos.");
    return ESP_OK;
}

esp_err_t display_show_splash(esp_lcd_panel_handle_t panel_handle, const uint16_t *image_data) {
    if (panel_handle == NULL || image_data == NULL) return ESP_ERR_INVALID_ARG;
    return esp_lcd_panel_draw_bitmap(panel_handle, 0, 0, LCD_H_RES, LCD_V_RES, image_data);
}

esp_err_t display_hal_sleep(esp_lcd_panel_handle_t panel_handle) {
    if (panel_handle == NULL) return ESP_ERR_INVALID_ARG;
    ESP_LOGI(TAG, "Iniciando protocolo de blindagem eletrica para Deep Sleep...");
    
    // Desliga o Backlight e aplica Sleep IN no driver
    gpio_set_level(PIN_NUM_BL, 0);
    esp_lcd_panel_disp_on_off(panel_handle, false);
    vTaskDelay(pdMS_TO_TICKS(20)); // Aguarda o comando chegar antes de cortar o clock

    // 2. Corta fisicamente o VCC da tela pelo Transistor PNP (HIGH = Corta)
    gpio_set_level(PIN_NUM_TRANSISTOR, 1);
    
    /* 3. Prevenção de Backpowering (Dreno Fantasma)
     Puxa as linhas de dados para GND (0V) para não vazar corrente pelo barramento SPI */
    gpio_set_level(PIN_NUM_MOSI, 0);
    gpio_set_level(PIN_NUM_CLK,  0);
    gpio_set_level(PIN_NUM_CS,   1);
    gpio_set_level(PIN_NUM_DC,   0);
    gpio_set_level(PIN_NUM_RST,  0);
    
    /* 4. Ativa a Retenção (Hold) do domínio RTC
     Isso garante que o ESP32-S3 mantenha esses pinos travados nesses níveis (GND e VCC cortado)
     durante todo o tempo em que estiver no Deep Sleep. */
    gpio_hold_en(PIN_NUM_TRANSISTOR);
    gpio_hold_en(PIN_NUM_BL);
    gpio_hold_en(PIN_NUM_MOSI);
    gpio_hold_en(PIN_NUM_CLK);
    gpio_hold_en(PIN_NUM_CS);
    gpio_hold_en(PIN_NUM_DC);
    gpio_hold_en(PIN_NUM_RST);

    return ESP_OK;
}