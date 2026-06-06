---
tipo: agent-reference
status: vigente
area: meta
ultima-atualizacao: 2026-05-27
---

# CANONICAL_INDEX — Verdade vigente por área

> Cada linha aponta para a nota (ou arquivo de código) que é a fonte de
> verdade ATUAL sobre um assunto. Se houver conflito, esta é a fonte que
> prevalece sobre todas as outras notas, memórias e PDFs.

## Regra de leitura

Coluna **assunto**: pergunta que você (agente) faria em sessão fresca.
Coluna **canonical**: arquivo a abrir.
Coluna **fallback**: se canonical não responder, próxima fonte.

## Hardware

| assunto | canonical | fallback |
|---|---|---|
| Pinout GPIO atual | `components/hardware/include/board_pins.h` (código) | [[MOC_hardware]] |
| Calibração de cor do display | [[calibracao-cores-display]] | `components/hal_bridge/hal_bridge.c` |
| Driver ST7796 (display) | `components/hardware/display_hal.c` + `.h` | [[driver-st7796s]] (parcialmente obsoleta — `status: obsoleto`) |
| Driver PN532 (NFC) | `components/hardware/nfc_hal.c` + `.h` | [[MOC_hardware]] |
| Driver NAND W25N01GV (storage) | `components/hardware/storage_hal.c` + `.h` | [[MOC_hardware]] |
| Driver botões | `components/hardware/button_hal.c` + `.h` | [[MOC_hardware]] |
| Driver joystick | `components/hardware/joystick_hal.c` + `.h` | [[MOC_hardware]] |
| PMU (power management) | `components/hardware/pmu.c` + `pmu.h` | memória `project_display_pending_changes` |
| PCB (status, revisões) | memória `project_pcb_two_boards` | [[MOC_hardware]] — PCB descartada momentaneamente (2026-05-27) |
| Roadmap migração P4 | [[futuro-migrar-p4-JC4880P443]] | `CONSULTA/JC4880P443C_I_W/` |

## Firmware (HALs e arquitetura de componentes)

| assunto | canonical | fallback |
|---|---|---|
| Mapa de dependências entre componentes | seção 3 de [[diagramas-do-projeto]] | `components/*/CMakeLists.txt` |
| Contrato HAL ↔ LVGL | memória `hal_boundary_contract` + `components/hal_bridge/` | [[MOC_firmware]] |
| Engine + game loop | `components/engine/engine.c` | [[MOC_firmware]] |
| Recovery / USB CDC | `components/recovery/recovery.c` | memória `recovery` (se existir) |
| asset_store / NAND-FS | `components/asset_store/asset_store.c` | [[MOC_firmware]] |
| Coding standard ESP-IDF | memória `esp_idf_style_guide` | https://docs.espressif.com/projects/esp-idf/en/stable/esp32/contribute/style-guide.html |
| Padrões de design (HAL, drivers) | [[MOC_firmware]] §"Padrões de design" | livros em `CONSULTA/` (Beningo, Elecia) |

## FSM e game logic

| assunto | canonical | fallback |
|---|---|---|
| FSM macro (estados de alto nível) | seção 1 de [[diagramas-do-projeto]] | `components/fsm/include/fsm_states.h` |
| FSM sub-estados GAMEPLAY | seção 2 de [[diagramas-do-projeto]] | `components/fsm/fsm.c` |
| Tasks FreeRTOS + IPC | seção 5 de [[diagramas-do-projeto]] | [[matriz-reacao-ataques]] §"Tarefas concorrentes" |
| Lifecycle de boot | seção 7 de [[diagramas-do-projeto]] | `main/main.c` + `components/hardware/pmu.c` |
| Plano de implementação (etapas A-G) | [[plano-implementacao-game-logic]] | `CONSULTA/RESPOSTAS.txt` |

## Game design

| assunto | canonical | fallback |
|---|---|---|
| Matriz ataque × carta × HAL | [[matriz-reacao-ataques]] | `CONSULTA/Artigo.pdf` §3.2.1 |
| Topologia de salas | seção 6 de [[diagramas-do-projeto]] | [[matriz-reacao-ataques]] |
| Lógica dos 3 WS2812 | seção 9 de [[diagramas-do-projeto]] | memória `game_logic_decisions` |
| Z-ordering LVGL (5 camadas) | seção 10 de [[diagramas-do-projeto]] | [[matriz-reacao-ataques]] |
| Pipeline de assets (Aseprite → NAND) | seção 8 de [[diagramas-do-projeto]] | memória `pending_visual_logic_refactor` |
| Constantes de balanceamento MVP | [[matriz-reacao-ataques]] §"Regras de balanceamento" | `components/gamestate/include/game_config.h` (a criar) |
| Decisões finais MVP (RESPOSTAS) | [[DECISION_LOG]] | `CONSULTA/RESPOSTAS.txt` |

## Build, infra, ferramentas

| assunto | canonical | fallback |
|---|---|---|
| Como buildar / flashar | `tools/README.md` + `idf.py build` na raiz | [[MOC_firmware]] |
| Configuração ESP-IDF (sdkconfig) | `sdkconfig` | `sdkconfig.old` |
| Recovery USB workflow | `components/recovery/recovery.c` | CHANGELOG entries de bring-up (2026-05-26) |
| Script de auditoria | `tools/audit-project.ps1` | [[2026-05-26T2000-audit-routine]] |
| Skills personalizadas (/ajuda, /sync-consulta) | [[index-de-skills]] | `~/.claude/skills/` |
| Sistema de CHANGELOG | `CHANGELOG/README.md` | memória `skill_changelog_system` |

## Pedagogia (fundamentação acadêmica)

| assunto | canonical | fallback |
|---|---|---|
| Artigo original (fonte) | `CONSULTA/Artigo.pdf` | `CONSULTA/Artigo_extracted.txt` |
| Conceitos cobertos (Stallings, Kim&Solomon) | [[matriz-reacao-ataques]] §1 | `CONSULTA/Artigo.pdf` §2.3 |

## Externals (literatura técnica)

| assunto | canonical | fallback |
|---|---|---|
| HAL/driver patterns | `CONSULTA/pdfcoffee.com_reusable-firmware-development-4-pdf-free.pdf` (Beningo) | `.cache/pdf-text/reusable_firmware.txt` |
| Arquitetura embedded geral | `CONSULTA/Elecia White - O'Reilly Making Embedded Systems (2011).pdf` | `.cache/pdf-text/making_embedded.txt` |
| Style guide ESP-IDF | memória `esp_idf_style_guide` | URL oficial |
| Datasheet display ST7796 / MSP4030 | `CONSULTA/MSP4030_MSP4031_Specification_EN_V1.0.pdf` | `CONSULTA/msp_out.txt` |

## Como atualizar este index

Quando criar nota com `tipo: canonical`:
1. Adicionar linha na tabela da área correspondente.
2. Se substituir canonical anterior: incluir `replaces: [[antiga]]` na nota
   nova, mover antiga para `90_Historico/`, mudar `status: obsoleto`.
3. Atualizar `ultima-atualizacao` deste arquivo.
