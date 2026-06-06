---
tipo: historical
status: historico
area: firmware
data: 2026-05-08
arquivada-em: 2026-05-27
motivo-arquivamento: auditoria pré-implementação de 2026-05-08; toda a tabela de progresso (HALs 1-9) já está implementada hoje. Útil só como arqueologia.
tags: [cybersec, firmware, hal, fase2, auditoria]
---

> [!warning] NOTA HISTÓRICA — não usar para decisão atual
> Este documento descreve a auditoria pré-implementação da HAL feita em
> 2026-05-08. Os pinos e conflitos citados aqui **NÃO refletem o
> hardware atual**. Para pinout vigente: `components/hardware/include/board_pins.h`.
> Para estado atual dos HALs: [[MOC_hardware]].

# Auditoria da HAL Legada

> [!warning] Estado atual
> A HAL antiga em `components/hardware/` foi parcialmente apagada (ver `git status`: `D` em `Buton.c`, `Buton.h`, `joystick.c`, `joystick.h`). Apenas o **PMU** sobreviveu como driver minimamente saneado. A reconstrucao deve partir do PMU como referencia de estilo, mas com expansao radical para os demais perifericos.

## Contexto

Pinagem definitiva em [[20_Hardware_HAL/pinout-mestre|Pinout Mestre]] (ver memoria `project_hardware`). Hardware-alvo, conforme `CONSULTA/Artigo.pdf` Secao 3.3:

- **MCU**: ESP32-S3 N16R8 (Dual-Core LX7, 16 MB Flash, 8 MB PSRAM Octal)
- **Display**: TFT ST7796S 480x320 via **SPI** (compartilhado com NAND W25N01G)
- **NFC**: PN532 via **I2C**
- **Joystick**: analogico (ADC1 nos GPIO 1 e 2)
- **Botoes**: 4 push-buttons A/B/X/Y + PWR (ja coberto pelo PMU)
- **Audio**: buzzer passivo (PWM) - GPIO 45
- **LEDs**: WS2812 (RMT) - GPIO 48

## Inventario dos arquivos legados

### Deletados pelo `git rm` (rastreaveis em `git show 6f766c6`)

| Arquivo | Decisao | Justificativa |
|---|---|---|
| `components/hardware/Buton.c` | **DESCARTAR** | Typo no nome (`Buton` vs `Button`), API Arduino-style (`bool botao_pressionado(int pino)`), sem `esp_err_t`, sem debounce em software, sem queue para FreeRTOS |
| `components/hardware/Buton.h` | **DESCARTAR** | Define `BTN_UP=15, BTN_DOWN=16, BTN_LEFT=17, BTN_RIGHT=18` que **CONFLITAM** com pinout definitivo (GPIO 15 = Botao Y, GPIO 16 = NFC IRQ, GPIO 17 = Display CS, GPIO 18 = Display DC) |
| `components/hardware/joystick.c` | **DESCARTAR** | Deletado em commit anterior, sem conteudo recuperavel relevante |
| `components/hardware/joystick.h` | **DESCARTAR** | Mesma situacao |

### Sobreviventes em `components/hardware/`

| Arquivo | Decisao | Justificativa |
|---|---|---|
| `components/hardware/pmu.c` (76 linhas) | **REAPROVEITAR como referencia de estilo** | Codigo limpo: usa `esp_err_t`, `ESP_RETURN_ON_ERROR`, `ESP_LOG*`, `vTaskDelay`, padroes ESP-IDF nativos. Implementa boot-hold + shutdown monitor com FreeRTOS task |
| `components/hardware/include/pmu.h` (22 linhas) | **REAPROVEITAR** | Header bem estruturado: `#pragma once`, guarda `extern "C"`, definicoes claras (`PMU_PIN_PWR=4`, `PMU_HOLD_BOOT_MS=2000`, `PMU_HOLD_SHUTDOWN_MS=4000`) |
| `components/hardware/CMakeLists.txt` | **EXPANDIR** | Hoje so registra `pmu.c` com `REQUIRES esp_driver_gpio esp_hw_support freertos log`. Precisara incluir os novos drivers e dependencias (esp_lcd, driver_spi_master, driver_i2c, esp_adc, driver_ledc, driver_rmt) |

### Esqueletos vazios (apenas `.gitkeep`)

| Diretorio | Decisao |
|---|---|
| `components/engine/` | **RENOMEAR -> `components/game_logic/`** (per requisito da reconstrucao) |
| `components/fsm/` | **MERGE em `components/game_logic/`** (FSM e parte do motor) |
| `components/ui/` | **RENOMEAR -> `components/game_ui/`** |

### Camadas a CRIAR do zero

- `components/hardware/` (nova HAL completa: display, nfc, joystick, buttons, buzzer, leds)
- `components/hal_bridge/` (traducao sinal de hardware -> evento de jogo, conforme parte 2 do briefing)

## Padrao de qualidade ditado pelo PMU

Toda nova funcao da HAL deve seguir o estilo de `components/hardware/pmu.c`:

1. Retornar `esp_err_t` em init/configuracao
2. Usar `ESP_RETURN_ON_ERROR` para propagar falhas
3. Tag de log estatica (`static const char *TAG = "MOD";`)
4. Uso de `gpio_config_t` struct com inicializacao explicita
5. Tasks FreeRTOS com nome legivel (`pmu_monitor`) e stack adequada (3072 bytes minimo para tasks com log)
6. Header com `#pragma once` + `extern "C"` para compatibilidade C++

## Conflitos de pinagem documentados

A HAL legada (`Buton.h`) confirmava o motivo da quebra: alocava GPIOs **ja reservados** pelo barramento SPI/NFC/Display:

| GPIO | Uso definitivo (per memoria) | Uso na HAL legada | Resultado |
|---|---|---|---|
| 15 | Livre (era Botao Y antes do remapeamento de 2026-05-08) | BTN_UP | Sem conflito hoje, mas legacy continua incompativel pelos pinos abaixo |
| 16 | NFC IRQ | BTN_DOWN | Quebra do NFC |
| 17 | Display CS | BTN_LEFT | Quebra do display |
| 18 | Display DC | BTN_RIGHT | Quebra do display |

Conclusao: **a HAL legada era estruturalmente incompativel com o hardware fisico definitivo**. A reconstrucao do zero nao e capricho, e necessidade.

## Plano de reconstrucao — PROGRESSO 2026-05-08

| # | HAL | Status | Commit | Notas |
|---|---|---|---|---|
| 1 | **PMU** (`pmu.c`) | ✅ DONE | `7597aa6` | Boot hold 2s + Deep Sleep com `rtc_gpio_init` antes do `ext0_wakeup` (bug RTC setup corrigido vs versao CONSULTA) |
| 2 | **Button** (`button_hal.c`) | ✅ DONE | `1662116` | A/B/X/Y em GPIOs **11-14** (remapeado de 12-15 para ficarem contiguos), ISR + `xTimerResetFromISR` + queue de eventos |
| 3 | **Joystick** (`joystick_hal.c`) | ✅ DONE | `504e293` | ADC1 GPIO1/2, task no core 1, MA8 + deadzone 5% + calibracao com sanidade (cai pro default 2048 se centro fora de 1500-2500) |
| 4 | **NFC** (`nfc_hal.c`) | ✅ DONE | `efa3697` | PN532 via I2C, protocolo proprio (frame builder + ACK + parser), IRQ-driven, `start/stop_scanning` API. RFConfiguration `MxRtyPassiveActivation=1` resolve o bug de detect lento |
| 5 | **hal_common** (`hal_common.c`) | ✅ DONE | `efa3697` | Helper `hal_isr_service_install_once()` para evitar log "already installed" entre HALs |
| 6 | **Display** (`display_hal.c`) | 🟡 EM WORKING TREE (nao commitado) | — | ST7796S funciona em bench. **Atualizado 2026-05-10**: transistor agora e NPN no GPIO 42 (1=LIGA / 0=CORTA), controle no init ja implementado, mirror corrigido para (true,true). **Pendente**: `display_hal_sleep` anti-backpowering + atualizar comentario stale na linha 20 que ainda fala "PNP". |
| 7 | **Feedback** (`feedback_hal.c`) | ⚪ NAO INICIADO | — | Buzzer (LEDC PWM no GPIO 45) + WS2812 (RMT no GPIO 48) |
| 8 | **Storage** (`storage_hal.c`) | ⚪ NAO INICIADO | — | NAND W25N01GV no SPI compartilhado com display (CS=10, MISO=9) |
| 9 | **`hal_bridge/`** | ⚪ NAO INICIADO | — | Ponte HAL ↔ LVGL. Depende do display estar finalizado |

`main.c` integra os 4 HALs prontos + tem **scaffold temporario** (`nfc_test_trigger_task`) que liga/desliga varredura NFC enquanto BTN_A pressionado — sera removido quando `game_logic` existir.

## HALs legadas adicionadas em CONSULTA/ (2026-05-08)

O usuario adicionou em `CONSULTA/` uma copia funcional dos HALs de uma iteracao anterior do hardware. **NAO sao para portar diretamente** (pinout incompativel com a rev atual), mas servem como REFERENCIA ARQUITETURAL:

| Arquivo CONSULTA | Padrao util a adotar | Onde aplicar |
|---|---|---|
| `pmu.c` | Filtro temporal de boot 5s, enum `pmu_boot_mode_t` (3 modos) | Versao atual ja e mais limpa, nao portar |
| `display_hal.c` (`display_flush_cb`) | Callback de flush agnostico (recebe coords cruas + buffer, sem tipos LVGL) | Confirmar que `hal_bridge` use o mesmo padrao |
| `display_hal.c` (`display_hal_sleep`) | **Protocolo anti-backpowering**: puxar MOSI/CLK/CS/DC/RST para GND, `gpio_hold_en`, cortar VCC. **Inverter polaridade** ao adaptar: hardware atual e NPN, entao GPIO 42=0 corta (CONSULTA usa PNP=1) | Adicionar ao `display_hal.c` atual quando integrar com PMU sleep |
| `display_hal.c` (transistor NPN GPIO 42) | Controle de VCC fisico do display. **Hardware substituido para NPN em 2026-05-10**: 1=LIGA, 0=CORTA (inverso do CONSULTA, que e PNP). Controle no init ja foi adicionado pelo usuario; falta apenas `display_hal_sleep`. Detalhes em memoria `project_display_pending_changes` |
| `button_hal.c` | ISR + `xTimerResetFromISR` + queue FreeRTOS para debounce 50ms | Adotar ao reconstruir `input_hal.c`. **Trocar pinos** para A=12, B=13, X=14, Y=15 |
| `joystick_hal.c` | Task dedicada (core 1), moving average 8 amostras, calibracao 500ms inicial, deadzone 5% | Adotar em `input_hal.c`. **Trocar para ADC1** canais nos GPIO 1/2 |

Conflitos detalhados de pinout estao em [[log-sincronizacao-consulta]] (sync de 2026-05-08). Nenhuma sobreposicao de memoria foi necessaria — `project_hardware` ja esta correto.

## Referencias consultadas

- `CONSULTA/Artigo.pdf` Secao 2.5 (Sistemas Embarcados de Baixo Custo) - justifica escolha ESP32-S3 + FreeRTOS
- `CONSULTA/Artigo.pdf` Secao 3.3 (Arquitetura de Hardware) - tabela de componentes
- `CONSULTA/Artigo.pdf` Secao 3.4 (Arquitetura de Software) - 4 tarefas concorrentes (Jogo, Grafica, NFC, Audio)
- `CONSULTA/{pmu,display_hal,button_hal,joystick_hal}.{c,h}` - HAL funcional anterior, hardware antigo, padroes arquiteturais reaproveitaveis
- Memoria local `project_hardware` - pinout definitivo confirmado em 2026-05-07
