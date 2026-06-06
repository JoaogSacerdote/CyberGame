#pragma once

#include <stdint.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Numero de LEDs WS2812 fisicos na placa (decisao de game design 2026-05-12:
 * LED1 = tarefa verde, LED2 = tarefa amarela, LED3 = alerta vermelho). */
#define WS2812_LED_COUNT  3

/* Inicializa a fita WS2812 (RMT no GPIO BOARD_PIN_WS2812_DATA). Idempotente.
 * Deixa todos os LEDs apagados. */
esp_err_t ws2812_hal_init(void);

/* Escreve a cor de UM LED no buffer interno. NAO acende ate ws2812_hal_refresh.
 * idx deve ser < WS2812_LED_COUNT (assert: violacao e bug do chamador). */
esp_err_t ws2812_hal_set_pixel(uint8_t idx, uint8_t r, uint8_t g, uint8_t b);

/* Escreve a mesma cor em TODOS os LEDs no buffer. NAO acende ate refresh. */
esp_err_t ws2812_hal_set_all(uint8_t r, uint8_t g, uint8_t b);

/* Zera o buffer (todos apagados). NAO acende ate refresh. */
esp_err_t ws2812_hal_clear(void);

/* Empurra o buffer interno para a fita fisica via RMT. */
esp_err_t ws2812_hal_refresh(void);

#ifdef __cplusplus
}
#endif
