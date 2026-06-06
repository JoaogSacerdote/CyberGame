#pragma once

#include "driver/gpio.h"

#ifdef __cplusplus
extern "C" {
#endif

/* === CyberGame board pin map ====================================================
 *
 * Mapa unico de GPIOs do console portatil ESP32-S3, agrupado por subsistema
 * fisico. Esta e a unica fonte de verdade: HALs em components/hardware/
 * importam destes defines, nunca declaram pinos proprios.
 *
 * Agrupamento atual reflete o layout do protoboard (PCB ainda nao fechada):
 *   - ADC1 (joystick) ............ GPIO  1, 2     (ADC1_CH0, ADC1_CH1)
 *   - Botoes frontais ............ GPIO  9..13    (sequenciais)
 *   - PMU (power button) ......... GPIO 14
 *   - NFC PN532 (I2C) ............ GPIO  4, 39, 40
 *   - Display ST7796 (SPI2) ...... GPIO  5, 6, 7, 15, 16, 17, 42
 *   - microSD (SPI2, slot do modulo) GPIO 15, 16, 18, 47
 *
 * Se mudar pinout, edite SO este arquivo. Atencao aos pinos compartilhados
 * (DISP/SD via SPI2): mude os dois lados juntos.
 * ============================================================================ */

/* --- Joystick analogico (ADC1 oneshot) -------------------------------------- */
#define BOARD_PIN_JOY_X_ADC          GPIO_NUM_1   /* ADC1_CH0 */
#define BOARD_PIN_JOY_Y_ADC          GPIO_NUM_2   /* ADC1_CH1 */

/* --- Botoes frontais -------------------------------------------------------- */
#define BOARD_PIN_BTN_A              GPIO_NUM_9
#define BOARD_PIN_BTN_X              GPIO_NUM_10
#define BOARD_PIN_BTN_B              GPIO_NUM_11
#define BOARD_PIN_BTN_Y              GPIO_NUM_12
#define BOARD_PIN_BTN_START          GPIO_NUM_13

/* --- PMU (Power Management Unit) -------------------------------------------- */
#define BOARD_PIN_PMU_PWR            GPIO_NUM_14  /* power button + EXT0 wakeup */

/* --- NFC PN532 (I2C) -------------------------------------------------------- */
#define BOARD_PIN_NFC_SCL            GPIO_NUM_4
#define BOARD_PIN_NFC_SDA            GPIO_NUM_39
#define BOARD_PIN_NFC_IRQ            GPIO_NUM_40

/* --- Display ST7796 (SPI2_HOST) --------------------------------------------- */
#define BOARD_PIN_DISP_CS            GPIO_NUM_5
#define BOARD_PIN_DISP_RST           GPIO_NUM_6
#define BOARD_PIN_DISP_DC            GPIO_NUM_7
#define BOARD_PIN_DISP_MOSI          GPIO_NUM_15  /* compartilhado com microSD */
#define BOARD_PIN_DISP_SCK           GPIO_NUM_16  /* compartilhado com microSD */
#define BOARD_PIN_DISP_BL            GPIO_NUM_17  /* backlight via LEDC PWM */
#define BOARD_PIN_DISP_PWR_EN        GPIO_NUM_42  /* NPN driver: 1=liga VCC, 0=corta */

/* --- Feedback ao jogador (LED enderecavel + buzzer) ------------------------- */
#define BOARD_PIN_WS2812_DATA        GPIO_NUM_8   /* 3 LEDs WS2812 via RMT */
#define BOARD_PIN_BUZZER             GPIO_NUM_21  /* piezo passivo via LEDC PWM */

/* --- Cartao microSD (slot embutido no modulo de display, SPI2_HOST) ---------
 * Compartilha MOSI/SCK/MISO com o display no mesmo barramento SPI2_HOST.
 * Pinos do modulo LCDWIKI: 9 SDO(MISO)->GPIO18, 14 SD_CS->GPIO47. */
#define BOARD_PIN_SD_MOSI            GPIO_NUM_15  /* === DISP_MOSI */
#define BOARD_PIN_SD_SCK             GPIO_NUM_16  /* === DISP_SCK */
#define BOARD_PIN_SD_MISO            GPIO_NUM_18  /* compartilhado no SPI2_HOST */
#define BOARD_PIN_SD_CS              GPIO_NUM_47

#ifdef __cplusplus
}
#endif
