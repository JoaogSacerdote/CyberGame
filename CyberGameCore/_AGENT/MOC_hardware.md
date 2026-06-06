---
tipo: moc
status: vigente
area: hardware
ultima-atualizacao: 2026-05-27
---

# MOC — Hardware

> Mapa de tudo que é hardware. Entry point quando a pergunta envolve
> pinout, periféricos físicos, calibração, PCB, ou bring-up.

## Estado em uma linha

ESP32-S3 (N16R8 / PSRAM desabilitada por instabilidade no protoboard
atual). PCB descartada momentaneamente; pinout vive no protoboard +
`board_pins.h`. Display ST7796 480×320 + NAND W25N01GV compartilhando
SPI2. NFC PN532 via I2C. Joystick analógico ADC1.

## Pinout

| onde | conteúdo |
|---|---|
| ⭐ **Canonical**: `components/hardware/include/board_pins.h` | mapa físico vigente, com `_Static_assert` de invariantes |
| memória `project_pinout_revisao_2026_05_21` | snapshot antigo — pode estar stale |
| memória `project_hardware` | snapshot antigo — pode estar stale |
| [[2026-05-27T1039-board-pins-centralizacao]] | changelog da consolidação |

⚠️ **Notas antigas com pinout INCORRETO** (não confiar):
- [[driver-st7796s]] cita MOSI=7, SCK=8, CS=17, DC=18, RST=21, BL=38 — todos errados hoje.

## Periféricos por arquivo de código

| periférico | header | source | observações |
|---|---|---|---|
| Botões A/B/X/Y/START | `components/hardware/include/button_hal.h` | `components/hardware/button_hal.c` | START dual-use com PMU_REC (GPIO 13) |
| Joystick analógico | `components/hardware/include/joystick_hal.h` | `components/hardware/joystick_hal.c` | ADC1, deadzone 5%, MA8 |
| NFC PN532 | `components/hardware/include/nfc_hal.h` | `components/hardware/nfc_hal.c` | I2C, IRQ-driven, RFConfig MxRty=1 |
| Display ST7796 | `components/hardware/include/display_hal.h` | `components/hardware/display_hal.c` | SPI2_HOST 40 MHz, BGR, byte swap |
| Storage NAND | `components/hardware/include/storage_hal.h` | `components/hardware/storage_hal.c` | SPI2_HOST 50 MHz, W25N01GV (128 MB) |
| PMU (power) | `components/hardware/include/pmu.h` | `components/hardware/pmu.c` | Boot hold 2s + Deep Sleep ext0 wake |
| hal_common (ISR helper) | `components/hardware/include/hal_common.h` | `components/hardware/hal_common.c` | `hal_isr_service_install_once()` |

## Bridges e camadas acima

| componente | função |
|---|---|
| `components/hal_bridge/` | LVGL ↔ display_hal (byte_swap + R-boost para calibração) |
| `components/asset_store/` | abstração sobre NAND para leitura de sprites |
| `components/recovery/` | USB CDC PING/PONG (futuramente PUT/GET de assets) |

## Calibração

| tema | nota canonical |
|---|---|
| Pipeline de cor (byte swap + BGR + R-boost) | [[calibracao-cores-display]] |
| Resíduo conhecido (cinzas com leve tinge) | `CONSULTA/A resolver.txt` item D1 |

## PCB

| item | onde |
|---|---|
| Status atual | DESCARTADA MOMENTANEAMENTE (2026-05-27) — protoboard ativo |
| Plano original (2 placas) | memória `project_pcb_two_boards` |
| Esquemático KiCad | `hardware/esquematico/` |
| Migração futura (placa P4) | [[futuro-migrar-p4-JC4880P443]] |

## Quando hardware quebra (bring-up)

1. Verificar `CHANGELOG/entries/` por mudança recente que possa ter
   regredido — especialmente entries com prefixo "bring-up" ou "fix-".
2. Rodar `tools/audit-project.ps1` se suspeitar de inconsistência.
3. Modo bring-up: HALs toleram falha (LOGW + continua) — ver
   [[2026-05-26T1945-bring-up-mode-completo]].

## Pendências de hardware

- [[OPEN_QUESTIONS]] §hardware — itens em aberto consolidados.
- `display_hal_sleep` ainda não implementado (memória `project_display_pending_changes`).

## Externals relevantes

- `CONSULTA/MSP4030_MSP4031_Specification_EN_V1.0.pdf` — datasheet do
  módulo display.
- `CONSULTA/consulta_esquema_eletrico.md` — esquemas.
- `CONSULTA/JC4880P443C_I_W/` — docs da placa P4 (migração futura).
