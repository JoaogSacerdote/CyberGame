# Pinout do CyberGame ESP32-S3

> Fonte de verdade: `components/hardware/include/board_pins.h`
> Última atualização: 2026-05-27

## Joystick analógico (ADC1)

| GPIO | Função | Notas |
|---|---|---|
| 1 | JOY_X | ADC1_CH0 |
| 2 | JOY_Y | ADC1_CH1 |

## Botões frontais

| GPIO | Função | Notas |
|---|---|---|
| 9 | BTN_A | input pull-up |
| 10 | BTN_X | input pull-up |
| 11 | BTN_B | input pull-up |
| 12 | BTN_Y | input pull-up |
| 13 | BTN_START | input pull-up; **dual-use com PMU_REC** |

## PMU (Power Management Unit)

| GPIO | Função | Notas |
|---|---|---|
| 13 | PMU_REC | recovery button; **mesmo pino do BTN_START** (PMU lê no boot, button_hal lê após) |
| 14 | PMU_PWR | power button + EXT0 wakeup |

## NFC PN532 (I2C)

| GPIO | Função | Notas |
|---|---|---|
| 4 | NFC_SCL | I2C clock |
| 39 | NFC_SDA | I2C data |
| 40 | NFC_IRQ | interrupt out |

## Display ST7796 (SPI2_HOST)

| GPIO | Função | Notas |
|---|---|---|
| 5 | DISP_CS | chip select |
| 6 | DISP_RST | reset ativo-baixo |
| 7 | DISP_DC | data/command select |
| 15 | DISP_MOSI | SPI MOSI; **compartilhado com NAND** |
| 16 | DISP_SCK | SPI clock; **compartilhado com NAND** |
| 17 | DISP_BL | backlight (LEDC PWM 10 kHz) |
| 42 | DISP_PWR_EN | NPN driver: 1=liga VCC, 0=corta |

## Storage NAND W25N01GV (SPI2_HOST, compartilha bus com display)

| GPIO | Função | Notas |
|---|---|---|
| 15 | NAND_MOSI | **mesmo pino do DISP_MOSI** |
| 16 | NAND_SCK | **mesmo pino do DISP_SCK** |
| 18 | NAND_MISO | exclusivo NAND |
| 41 | NAND_CS | chip select |

## Resumo por GPIO

| GPIO | Quem usa | Subsistema |
|---|---|---|
| 1 | JOY_X | Joystick |
| 2 | JOY_Y | Joystick |
| 4 | NFC_SCL | NFC |
| 5 | DISP_CS | Display |
| 6 | DISP_RST | Display |
| 7 | DISP_DC | Display |
| 9 | BTN_A | Botões |
| 10 | BTN_X | Botões |
| 11 | BTN_B | Botões |
| 12 | BTN_Y | Botões |
| 13 | BTN_START + PMU_REC | Botões + PMU (dual-use) |
| 14 | PMU_PWR | PMU |
| 15 | DISP_MOSI + NAND_MOSI | SPI2 (compartilhado) |
| 16 | DISP_SCK + NAND_SCK | SPI2 (compartilhado) |
| 17 | DISP_BL | Display |
| 18 | NAND_MISO | NAND |
| 39 | NFC_SDA | NFC |
| 40 | NFC_IRQ | NFC |
| 41 | NAND_CS | NAND |
| 42 | DISP_PWR_EN | Display |

## Invariantes garantidos por `_Static_assert` em board_pins.h

- `BOARD_PIN_BTN_START == BOARD_PIN_PMU_REC` (dual-use deliberado)
- `BOARD_PIN_NAND_MOSI == BOARD_PIN_DISP_MOSI` (bus SPI2 compartilhado)
- `BOARD_PIN_NAND_SCK == BOARD_PIN_DISP_SCK` (idem)

Mudar pino em `board_pins.h` sem respeitar esses invariantes quebra o build.

## Notas de hardware

- **Protoboard atual** (PCB descartada momentaneamente em 2026-05-27).
- **Bus SPI2 compartilhado**: Display é dono (`spi_bus_initialize`); NAND tolera `ESP_ERR_INVALID_STATE` e usa `spi_bus_add_device`.
- **Backlight do display** via LEDC channel 0, timer 0, duty 13-bit, freq 10 kHz.
- **Brownout Detector** ativo no nível 7 (max) — ver `sdkconfig` `CONFIG_BROWNOUT_DET_LVL=7`.
- Esquemático KiCad em `hardware/esquematico/` (referência visual).
