---
date: 2026-05-26 19:30
type: edit
files:
  - components/hardware/button_hal.c
  - components/hardware/include/pmu.h
  - components/hardware/display_hal.c
  - components/hardware/storage_hal.c
  - components/hardware/nfc_hal.c
session: bring-up placa nova
---

# Reaplicação do pinout novo (placa nova, sucessor do 2026-05-21)

## Motivo
Após `git reset --hard HEAD` (entrada T1815) ter descartado o pinout novo, os HALs voltaram ao pinout antigo (que não bate com a placa nova nem com o schematic KiCad em `hardware/esquematico/`). Usuário ditou o pinout novamente, ligeiramente diferente da memória `project_pinout_revisao_2026_05_21` em um pino (NFC SDA: 38 → 39).

## Tabela de pinos

| Função | Pino antigo (HEAD) | Pino novo |
|---|---|---|
| Botão A | 11 | **9** |
| Botão B | 12 | **11** |
| Botão X | 13 | **10** |
| Botão Y | 14 | **12** |
| Botão START / REC | 3 (strap!) | **13** |
| PWR | 4 | **14** |
| Display MOSI | 7 | **15** |
| Display SCK | 8 | **7** |
| Display CS | 17 | **18** |
| Display DC | 18 | **16** |
| Display RST | 21 | **17** |
| Display BL (backlight) | 38 | **6** |
| Display PWR_EN (transistor) | 42 | 42 (igual) |
| Display MISO (hardcoded) | 9 | **5** |
| NAND MOSI | 7 | **15** |
| NAND SCK | 8 | **7** |
| NAND MISO | 9 | **5** |
| NAND CS | 10 | **41** |
| NFC SDA | 5 | **39** ⚠️ |
| NFC SCL | 6 | **4** |
| NFC IRQ | 16 | **40** |
| LED WS2812 | (sem HAL) | 8 |
| Buzzer | (sem HAL) | 21 |

⚠️ **Divergência da memória**: `project_pinout_revisao_2026_05_21` dizia NFC SDA=38 ("corrigido era 39"). Usuário ditou 39 nesta sessão. **39 é o valor verdadeiro agora.** Memória precisa ser atualizada.

## Antes

### `button_hal.c` (linhas 13-17)
```c
#define BTN_GPIO_A          GPIO_NUM_11
#define BTN_GPIO_B          GPIO_NUM_12
#define BTN_GPIO_X          GPIO_NUM_13
#define BTN_GPIO_Y          GPIO_NUM_14
#define BTN_GPIO_START      GPIO_NUM_3   /* compartilhado com REC do PMU */
```

### `pmu.h` (linhas 7-8)
```c
#define PMU_PIN_PWR             GPIO_NUM_4
#define PMU_PIN_REC             GPIO_NUM_3
```

### `display_hal.c` (linhas 14-20)
```c
#define DISP_PIN_MOSI       7
#define DISP_PIN_SCK        8
#define DISP_PIN_CS         17
#define DISP_PIN_DC         18
#define DISP_PIN_RST        21
#define DISP_PIN_BL         38
#define DISP_PIN_PWR_EN     42
```
E linha hardcoded em `display_hal_spi_bus_init`: `.miso_io_num = 9`

### `storage_hal.c` (linhas 14-17)
```c
#define STORAGE_PIN_MOSI       7
#define STORAGE_PIN_SCK        8
#define STORAGE_PIN_MISO       9
#define STORAGE_PIN_CS         10
```

### `nfc_hal.c` (linhas 16-18)
```c
#define NFC_PIN_SDA             GPIO_NUM_5
#define NFC_PIN_SCL             GPIO_NUM_6
#define NFC_PIN_IRQ             GPIO_NUM_16
```

## Depois

### `button_hal.c`
```c
#define BTN_GPIO_A          GPIO_NUM_9
#define BTN_GPIO_B          GPIO_NUM_11
#define BTN_GPIO_X          GPIO_NUM_10
#define BTN_GPIO_Y          GPIO_NUM_12
#define BTN_GPIO_START      GPIO_NUM_13  /* compartilhado com REC do PMU */
```

### `pmu.h`
```c
#define PMU_PIN_PWR             GPIO_NUM_14
#define PMU_PIN_REC             GPIO_NUM_13
```

### `display_hal.c`
```c
#define DISP_PIN_MOSI       15
#define DISP_PIN_SCK        7
#define DISP_PIN_CS         18
#define DISP_PIN_DC         16
#define DISP_PIN_RST        17
#define DISP_PIN_BL         6
#define DISP_PIN_PWR_EN     42
```
E linha hardcoded: `.miso_io_num = 5`

### `storage_hal.c`
```c
#define STORAGE_PIN_MOSI       15
#define STORAGE_PIN_SCK        7
#define STORAGE_PIN_MISO       5
#define STORAGE_PIN_CS         41
```

### `nfc_hal.c`
```c
#define NFC_PIN_SDA             GPIO_NUM_39
#define NFC_PIN_SCL             GPIO_NUM_4
#define NFC_PIN_IRQ             GPIO_NUM_40
```

## Resultado
Não testado. Buzzer (GPIO 21) e WS2812 (GPIO 8) ainda sem HAL. SPI2 compartilhado Tela+NAND (MOSI=15, SCK=7, MISO=5).

## Validações pendentes
- Todos os GPIOs no S3 são válidos para uso geral
- Joystick não foi mexido (já estava em ADC_CHANNEL_0/1 = GPIO1/2)
- GPIO 3 (era REC/strap) liberado — sem mais conflito com strap pin no boot
- NAND CS=41 não-strapping (era GPIO 3 antes, conflito grave)

## Links
- Memória: [[project_pinout_revisao_2026_05_21]] (precisa atualizar SDA: 38 → 39)
- Entrada relacionada: [[2026-05-26T1815-git-reset-hard]] (perda anterior do mesmo trabalho)
