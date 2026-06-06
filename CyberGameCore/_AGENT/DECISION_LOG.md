---
tipo: agent-reference
status: vigente
area: meta
ultima-atualizacao: 2026-05-27
fonte-bruta: CONSULTA/RESPOSTAS.txt
---

# DECISION_LOG — Decisões fechadas (sumário)

> Cada linha = 1 decisão tomada, com data, motivo curto e ponteiro para
> onde foi materializada. Detalhe técnico mora em [[CANONICAL_INDEX]] /
> notas específicas. Diálogo bruto que originou: `CONSULTA/RESPOSTAS.txt`.
>
> Decisões em ordem cronológica reversa (mais recente em cima).

## 2026-05-27

### D-2026-05-27-04. Asserts vs ESP_ERR_INVALID_ARG nos HALs (G6)
- **Por quê**: distinguir bug de chamador (assert) de erro de runtime (esp_err).
  Sem isso, ESP_ERR mascara bugs que deveriam ser detectados em dev.
- **Regra**: `assert(ptr != NULL)` quando NULL é violação de contrato; manter
  `if (state == ...) return ESP_ERR_INVALID_STATE` para condição transiente.
- **Materializado em**: commit 8d25c48 + `docs/ai-rules/04-hal-contract.md` §asserts

### D-2026-05-27-05. Task watchdog no engine_task; button_reader_task fora (G8)
- **Por quê**: detectar travamento de FSM com rastro em log antes do reset.
- **Como**: `esp_task_wdt_add(NULL)` + `esp_task_wdt_reset()` no loop.
  button_reader_task **excluída** por bloquear indefinidamente por design.
- **Regra geral**: tasks com block infinito ficam fora do WDT.
- **Materializado em**: commit 2f87793 + `docs/ai-rules/05-freertos-rules.md` §watchdog

### D-2026-05-27-06. Semver manual em main/version.h (G9)
- **Por quê**: identificar build em campo + correlacionar com reset reason.
- **Como**: macros `CYBERGAME_VERSION_*` + banner ASCII em `app_main`.
  Build indicator git/hash adiado (`esp_app_desc.h` não disponível em IDF v6.0).
- **Materializado em**: commit af739e1

### D-2026-05-27-03. Vault reorganizado para uso por agente

### D-2026-05-27-01. Centralizar pinout em board_pins.h
- **Por quê**: auditoria contra Beningo/Elecia/ESP-IDF apontou pinout
  espalhado em 6 arquivos.
- **Como**: criado `components/hardware/include/board_pins.h` com macros
  `BOARD_PIN_*` + `_Static_assert` de invariantes.
- **Materializado em**: [[2026-05-27T1039-board-pins-centralizacao]]
- **Status**: ✅ build verde

### D-2026-05-27-02. PCB descartada momentaneamente
- **Por quê**: protoboard funcional, foco em firmware/gameplay
- **Reabrir quando**: MVP estável + decisão de "fechar produto"
- **Materializado em**: comentário em board_pins.h + [[MOC_hardware]]

### D-2026-05-27-03. Vault reorganizado para uso por agente
- **Por quê**: usuário pediu otimização para Claude Code, não para humano
- **Como**: criada pasta `_AGENT/` com entrypoint, vocab, MOCs, canonical index
- **Materializado em**: este vault (esta sessão)

## 2026-05-26

### D-2026-05-26-01. SPI compartilhado entre Display e NAND
- **Por quê**: economizar pinos no protoboard novo
- **Como**: SPI2_HOST com MOSI=15, SCK=16 compartilhados; CS distintos
- **Materializado em**: [[2026-05-26T2110-nand-share-spi-tela]]

### D-2026-05-26-02. PSRAM desabilitada
- **Por quê**: PSRAM Octal instável neste módulo causando boot loop
- **Como**: `CONFIG_SPIRAM=n` no sdkconfig
- **Materializado em**: [[2026-05-26T1830-disable-psram]]
- **Impacto**: framebuffer LVGL não tem mais 614 KB em PSRAM — usar strips menores

### D-2026-05-26-03. Modo bring-up: HALs toleram falha
- **Por quê**: bring-up de PCB nova, hardware pode estar incompleto
- **Como**: todos os HAL inits viram `LOGW + continua` em vez de abortar
- **Materializado em**: [[2026-05-26T1945-bring-up-mode-completo]]

## 2026-05-13

### D-2026-05-13-01. Pipeline de cor do display
- **Por quê**: cores totalmente erradas no bring-up inicial
- **Como**: `byte_swap_inplace` + `LCD_RGB_ELEMENT_ORDER_BGR` + `R_BOOST_MULT=2`
- **Materializado em**: [[calibracao-cores-display]]
- **Status**: ✅ validado empiricamente (24 swatches sRGB)

### D-2026-05-13-02. feedback_hal (3 WS2812 + buzzer) ADIADO para pós-MVP
- **Por quê**: foco em gameplay primeiro; outputs ficam só na tela
- **Impacto**: lógica dos 3 LEDs (escalada de ataque) sem efeito visível;
  6 SFX silenciados até feedback_hal existir
- **Materializado em**: `CONSULTA/A resolver.txt` E1

## 2026-05-12 (consolidação MVP — RESPOSTAS.txt)

### D-2026-05-12-01. Trio de ataques MVP: DDoS, Ransomware, Propagação Lateral
- **Por quê**: cobertura dos 3 pilares CIA (Disponibilidade, Integridade×2)
- **Adiado**: Phishing (4º ataque, pós-MVP)
- **Materializado em**: [[matriz-reacao-ataques]]

### D-2026-05-12-02. 3 tasks FreeRTOS (não 4 como o artigo propunha)
- **Por quê**: áudio não precisa de task própria — PWM/LEDC não-bloqueante
- **Tasks**: UI/LVGL (Core 1), GameEngine (Core 0), Hardware (Core 0)
- **Materializado em**: seção 5 de [[diagramas-do-projeto]]

### D-2026-05-12-03. 3 LEDs WS2812 no mesmo barramento RMT
- **Por quê**: minimizar pinos + simplificar gerenciamento
- **GPIO**: 48 (RMT)
- **Lógica**: LED1=verde, LED2=amarelo, LED3=vermelho; escalada arrasta os outros
- **Materializado em**: seção 9 de [[diagramas-do-projeto]] + memória `game_logic_decisions`

### D-2026-05-12-04. Pause via botão START; terminal via botão Y
- **Materializado em**: seção 1 + 2 de [[diagramas-do-projeto]]

### D-2026-05-12-05. Calibração NFC removida — UIDs hardcoded em nfc_config.h
- **Por quê**: simplificar fluxo de boot/MVP
- **Pendente**: B1 (usuário precisa ler 3 UIDs reais)

### D-2026-05-12-06. Tutorial dirigido REMOVIDO
- **Por quê**: aprendizagem orgânica via NPC recepcionista
- **Materializado em**: seção 1 de [[diagramas-do-projeto]]

### D-2026-05-12-07. Sistema de arquivos NAND: LittleFS
- **Por quê**: mais leve que FATFS, projetado para flash, suporte oficial ESP-IDF
- **Materializado em**: `A resolver.txt` T5 (formalizado aqui)

### D-2026-05-12-08. Expediente de 3 minutos reais (08:00→18:00 in-game)
- **Materializado em**: `EXPEDIENTE_DURACAO_MS` em [[matriz-reacao-ataques]]

## 2026-05-10

### D-2026-05-10-01. Transistor de PWR_EN do display: PNP → NPN
- **Por quê**: usuário substituiu fisicamente
- **Impacto**: polaridade inversa (1=LIGA, 0=CORTA)
- **Materializado em**: `components/hardware/display_hal.c` + memória `project_hardware`

## 2026-05-08

### D-2026-05-08-01. HAL legada DESCARTADA, reconstrução do zero
- **Por quê**: typos, API Arduino-style, pinos conflitando com SPI/I2C/Display
- **Materializado em**: [[auditoria-hal-legada]] (`status: historico`)

## Como adicionar decisão nova

1. Adicionar entrada no topo (mais recente em cima).
2. ID: `D-YYYY-MM-DD-NN` (NN incremental no dia).
3. Sempre incluir: **Por quê**, **Como/Onde**, **Materializado em** (link).
4. Se altera código: link para entrada de CHANGELOG.
5. Se altera contrato/arquitetura: link para nota canonical atualizada.
