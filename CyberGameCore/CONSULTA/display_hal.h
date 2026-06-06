#ifndef DISPLAY_HAL_H
#define DISPLAY_HAL_H

#pragma once

#include <stdint.h>
#include "esp_err.h"
#include "esp_lcd_panel_ops.h"

/**
 * @brief Inicializa a Válvula de Energia (PNP), o barramento SPI e o controlador ST7796S.
 * * @note Esta função gerencia o VCC físico da tela via PMU. 
 * @param[out] ret_panel Ponteiro para armazenar o handle do painel inicializado.
 * @return esp_err_t ESP_OK em caso de sucesso.
 */
esp_err_t display_hal_init(esp_lcd_panel_handle_t *ret_panel);

/**
 * @brief Callback de Flush agnóstico (Independente de Biblioteca Gráfica).
 * * Esta função recebe coordenadas brutas e um buffer de pixels. 
 * Ela "limpa" a dependência do LVGL ao usar apenas tipos primitivos.
 * * @param disp Ponteiro genérico para o driver (será castado internamente).
 * @param x1 Coordenada X inicial.
 * @param y1 Coordenada Y inicial.
 * @param x2 Coordenada X final.
 * @param y2 Coordenada Y final.
 * @param color_p Ponteiro para o buffer de cores (RGB565).
 */
void display_flush_cb(void *disp, int x1, int y1, int x2, int y2, uint8_t *color_p);

/**
 * @brief Exibe uma imagem bruta (Splash Screen) via DMA.
 * * @param panel_handle O handle do painel obtido no init.
 * @param image_data Ponteiro para o array de pixels em formato uint16_t (RGB565).
 * @return esp_err_t ESP_OK se o envio ao DMA foi bem sucedido.
 */
esp_err_t display_show_splash(esp_lcd_panel_handle_t panel_handle, const uint16_t *image_data);

/**
 * @brief Executa o "Protocolo de Blindagem" para Deep Sleep.
 * * Desliga o Backlight, corta o VCC via transistor PNP e coloca o ST7796S em modo Sleep.
 * * @param panel_handle O handle do painel LCD.
 * @return esp_err_t ESP_OK.
 */
esp_err_t display_hal_sleep(esp_lcd_panel_handle_t panel_handle);

#endif // DISPLAY_HAL_H