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
 *   - PMU (power button + rec) ... GPIO 13, 14    (REC compartilha com BTN_START)
 *   - NFC PN532 (I2C) ............ GPIO  4, 39, 40
 *   - Display ST7796 (SPI2) ...... GPIO  5, 6, 7, 15, 16, 17, 42
 *   - NAND W25N01GV (SPI2) ....... GPIO 15, 16, 18, 41 (MOSI/SCK compartilham
 *                                                       bus com o display)
 *
 * Se mudar pinout, edite SO este arquivo. Os _Static_assert ao final garantem
 * que invariantes de hardware (dual-use, bus compartilhado) sejam preservadas.
 * ============================================================================ */

/* --- Joystick analogico (ADC1 oneshot) -------------------------------------- */
#define BOARD_PIN_JOY_X_ADC          GPIO_NUM_1   /* ADC1_CH0 */
#define BOARD_PIN_JOY_Y_ADC          GPIO_NUM_2   /* ADC1_CH1 */

/* --- Botoes frontais -------------------------------------------------------- */
#define BOARD_PIN_BTN_A              GPIO_NUM_9
#define BOARD_PIN_BTN_X              GPIO_NUM_10
#define BOARD_PIN_BTN_B              GPIO_NUM_11
#define BOARD_PIN_BTN_Y              GPIO_NUM_12
#define BOARD_PIN_BTN_START          GPIO_NUM_13  /* dual-use: PMU_REC durante boot */

/* --- PMU (Power Management Unit) -------------------------------------------- */
#define BOARD_PIN_PMU_PWR            GPIO_NUM_14  /* power button + EXT0 wakeup */
#define BOARD_PIN_PMU_REC            GPIO_NUM_13  /* recovery; coincide com BTN_START */

/* --- NFC PN532 (I2C) -------------------------------------------------------- */
#define BOARD_PIN_NFC_SCL            GPIO_NUM_4
#define BOARD_PIN_NFC_SDA            GPIO_NUM_39
#define BOARD_PIN_NFC_IRQ            GPIO_NUM_40

/* --- Display ST7796 (SPI2_HOST) --------------------------------------------- */
#define BOARD_PIN_DISP_CS            GPIO_NUM_5
#define BOARD_PIN_DISP_RST           GPIO_NUM_6
#define BOARD_PIN_DISP_DC            GPIO_NUM_7
#define BOARD_PIN_DISP_MOSI          GPIO_NUM_15  /* compartilhado com NAND */
#define BOARD_PIN_DISP_SCK           GPIO_NUM_16  /* compartilhado com NAND */
#define BOARD_PIN_DISP_BL            GPIO_NUM_17  /* backlight via LEDC PWM */
#define BOARD_PIN_DISP_PWR_EN        GPIO_NUM_42  /* NPN driver: 1=liga VCC, 0=corta */

/* --- Storage NAND W25N01GV (SPI2_HOST, compartilha bus com display) --------- */
#define BOARD_PIN_NAND_MOSI          GPIO_NUM_15  /* === DISP_MOSI */
#define BOARD_PIN_NAND_SCK           GPIO_NUM_16  /* === DISP_SCK */
#define BOARD_PIN_NAND_MISO          GPIO_NUM_18
#define BOARD_PIN_NAND_CS            GPIO_NUM_41

/* === Invariantes de hardware =================================================
 * Estes asserts disparam em compile-time se alguem mudar um pino acima sem
 * pensar nas consequencias. NAO os remova para "fazer o build passar". */

_Static_assert(BOARD_PIN_BTN_START == BOARD_PIN_PMU_REC,
               "BTN_START e PMU_REC compartilham o mesmo GPIO por design "
               "(PMU le como REC durante boot; button_hal claim como START depois). "
               "Ver pmu.c e button_hal.c.");

_Static_assert(BOARD_PIN_NAND_MOSI == BOARD_PIN_DISP_MOSI,
               "NAND e Display compartilham MOSI no SPI2_HOST. Se separar, "
               "alocar bus distinto (SPI3_HOST) para um dos dois.");

_Static_assert(BOARD_PIN_NAND_SCK == BOARD_PIN_DISP_SCK,
               "NAND e Display compartilham SCK no SPI2_HOST. Mesma regra "
               "do MOSI acima.");

#ifdef __cplusplus
}
#endif
