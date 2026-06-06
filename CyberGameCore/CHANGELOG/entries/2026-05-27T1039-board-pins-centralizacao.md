---
data: 2026-05-27T10:39
tipo: refactor
escopo: components/hardware
build: passou (CyberGame.bin 0x8c6c0, -0 bytes vs anterior)
trigger: auditoria pos-estudo de Beningo + Elecia + ESP-IDF style guide
---

# Centralizacao de pinos GPIO em board_pins.h

## Por que

Apos estudar 3 fontes adicionais (Beningo "Reusable Firmware", Elecia "Making Embedded Systems", ESP-IDF style guide oficial), auditoria do projeto apontou: pinout espalhado em 6 arquivos diferentes (Elecia cap 4 recomenda "Board-Specific Header File" unico). Decisao do usuario: Pacote A (so pinout), header unico sem #ifdef de revisao (PCB descartada momentaneamente).

## O que mudou

**Criado:**
- `components/hardware/include/board_pins.h` — fonte unica de verdade, organizada por subsistema fisico, com _Static_assert para invariantes (dual-use BTN_START==PMU_REC, NAND/Display compartilhando MOSI+SCK no SPI2_HOST).

**Refatorados (incluem board_pins.h, removem defines locais):**
- `button_hal.c`: BTN_GPIO_* -> BOARD_PIN_BTN_*
- `joystick_hal.c`: mantem JOY_X/Y_CHANNEL (sao canais ADC, nao GPIOs), adiciona _Static_assert ligando-os a BOARD_PIN_JOY_*_ADC
- `nfc_hal.c`: NFC_PIN_* -> BOARD_PIN_NFC_*
- `display_hal.c`: DISP_PIN_* -> BOARD_PIN_DISP_* (7 pinos)
- `storage_hal.c`: STORAGE_PIN_* -> BOARD_PIN_NAND_*
- `pmu.c`: PMU_PIN_* -> BOARD_PIN_PMU_*
- `pmu.h` (header publico): removeu PMU_PIN_PWR/REC e `#include "driver/gpio.h"` — pinos nao sao parte do contrato publico do PMU

## Pinout (inalterado — so movido)

| GPIO | Funcao | Subsistema |
|------|--------|------------|
| 1, 2 | JOY_X, JOY_Y (ADC1_CH0/1) | Joystick |
| 4, 39, 40 | SCL, SDA, IRQ | NFC PN532 |
| 5, 6, 7 | CS, RST, DC | Display |
| 9, 10, 11, 12, 13 | BTN_A, X, B, Y, START | Botoes |
| 13 (compartilhado) | PMU_REC | PMU (dual-use com START) |
| 14 | PMU_PWR | PMU |
| 15, 16 | MOSI, SCK (SPI2) | Display + NAND |
| 17 | Backlight (LEDC) | Display |
| 18, 41 | MISO, CS | NAND |
| 42 | PWR_EN (NPN) | Display |

## Gotchas

- Erro de comentario na primeira tentativa: `(components/hardware/*.c)` dentro de bloco `/* */` — GCC interpretou `*/` como fim. Fix: escrever `components/hardware/` sem o glob.
- Diagnosticos do clangd LSP durante a refatoracao mostravam macros antigas e "Expected ')'" — eram cache stale + parser nao reconhecendo macros do esp_check.h. Sao falso positivo; build com xtensa-esp32s3-elf-gcc passa limpo.

## Como reverter (se precisar)

```
git diff HEAD -- components/hardware/ | git apply -R
rm components/hardware/include/board_pins.h
```

## Pendencias relacionadas (do mesmo audit, NAO feitas nesta rodada)

- G2: Input service abstraindo button/joystick/nfc (UI/FSM hoje tocam HAL direto)
- G3: Callback registration nos HALs de input
- G4: Init com config table (Beningo tip #9)
- G5: docs/ai-rules/ + Doxygen
- G6: Asserts/Design by Contract nos HALs
- G8: Task watchdog no engine_task
- G9: firmware_version.h imprimindo no boot
