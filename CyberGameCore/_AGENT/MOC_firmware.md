---
tipo: moc
status: vigente
area: firmware
ultima-atualizacao: 2026-05-27
---

# MOC — Firmware (HALs, componentes ESP-IDF, padrões)

> Mapa quando a pergunta envolve arquitetura de software embarcado: HALs,
> bridges, engine, recovery, build, dependencias entre componentes.

## Estado em uma linha

ESP-IDF v6.0, ESP32-S3, FreeRTOS. Camadas: `hardware` (HALs) →
`hal_bridge` (LVGL) → `engine` (game loop) → `ui` (telas LVGL). `gamestate`
puro C compartilhado. Build verde em 2026-05-27 (CyberGame.bin 0x8c6c0,
86% partição livre).

## Dependências entre componentes

⭐ **Canonical**: seção 3 de [[diagramas-do-projeto]] (Mermaid graph)
+ `components/*/CMakeLists.txt` (REQUIRES é a verdade técnica).

Regras de dependência respeitadas hoje (com gaps documentados em
[[OPEN_QUESTIONS]] §arquitetura):
- `hardware/*.h` **não** inclui LVGL. (regra `[[hal_boundary_contract]]`)
- `hal_bridge` adapta display_hal ↔ LVGL.
- UI/FSM/engine **vazam para HAL direto** (gap conhecido — [[OPEN_QUESTIONS]] §G2).

## Componentes ativos

| componente | função | canonical |
|---|---|---|
| `hardware/` | HALs de periféricos físicos | [[MOC_hardware]] |
| `hal_common/` | helper ISR service install once | `components/hardware/hal_common.c` |
| `hal_bridge/` | bridge LVGL ↔ display | `components/hal_bridge/hal_bridge.c` |
| `engine/` | game loop (FSM tick + button reader task + sync UI) | `components/engine/engine.c` |
| `fsm/` | máquina de estados macro + sub-estados gameplay | `components/fsm/fsm.c` |
| `gamestate/` | puro C: relógio expediente, vidas, score | `components/gamestate/gamestate.c` |
| `ui/` | telas LVGL (splash, menu, recepção, empresa, HUD, game_over, pause) | `components/ui/*.c` |
| `ui_debug/` | menu dev (combo Y+START segura 2s) | `components/ui_debug/ui_debug.c` |
| `asset_store/` | leitura de sprites da NAND | `components/asset_store/asset_store.c` |
| `assets/` | asset_loader + dialog_loader + collision maps | `components/assets/` |
| `recovery/` | USB CDC PING/PONG (extensível para PUT/GET) | `components/recovery/recovery.c` |

## Padrões de design adotados

| padrão | onde | fonte |
|---|---|---|
| Adapter (drivers stackable) | `hal_bridge` adapta `display_hal` ao LVGL | Elecia White cap 2 |
| Facade | `pmu_init/sleep`, `nfc_hal_start_scanning` | Elecia White cap 4 |
| ISR + queue + processing task | `button_hal` → `engine.button_reader_task` → queue → `engine_task` | Beningo cap 3 (Fig 3-25) |
| Callback registration | só `display_hal_register_trans_done_cb` (gap: input HALs não têm) | Beningo cap 6 step 3 |
| Board-specific header | `components/hardware/include/board_pins.h` (criado 2026-05-27) | Elecia White cap 4 |
| Static asserts para invariantes | `board_pins.h` (dual-use, bus compartilhado) | — |

## Coding standard

⭐ **Canonical**: memória `[[esp_idf_style_guide]]` (síntese da fonte oficial).

Regras práticas:
- 4 espaços, sem tabs.
- Linha máx 120 chars.
- `s_` prefix em static vars.
- `_t` suffix em types via typedef.
- `#pragma once` em headers (não macro guards).
- `extern "C"` block obrigatório em headers públicos.
- Ordem de includes: stdlib → POSIX → IDF → outros componentes → próprio.
- `ESP_ERROR_CHECK` / `ESP_RETURN_ON_ERROR` para `esp_err_t`.
- `assert()` só para erros internos não-recuperáveis.

## Build

| ação | comando |
|---|---|
| Build | `python C:\esp\v6.0\esp-idf\tools\idf.py build` |
| Flash | `python C:\esp\v6.0\esp-idf\tools\idf.py -p COM17 flash` (porta — ver CHANGELOG 2026-05-26T1700) |
| Limpar | `python C:\esp\v6.0\esp-idf\tools\idf.py fullclean` |
| Auditoria estática | `tools/audit-project.ps1` |
| sdkconfig | raiz do projeto; PSRAM desabilitada (CHANGELOG 2026-05-26T1830) |

Last green build: 2026-05-27, CyberGame.bin = 0x8c6c0 bytes.

## Logs e debug

- Padrão: `static const char *TAG = "<COMPONENT>"` + `ESP_LOG{I,W,E}` em PT-BR.
- ESP_LOGI dentro de `lv_lock` é PROIBIDO ([[feedback_lvgl_diff_gating]]).
- Run Time Stats / Task List: ainda não habilitados (gap [[OPEN_QUESTIONS]] §G8).

## Externals relevantes

- `CONSULTA/pdfcoffee.com_reusable-firmware-development-4-pdf-free.pdf`
  (Beningo) — HAL e drivers.
- `CONSULTA/Elecia White - O'Reilly Making Embedded Systems (2011).pdf` —
  arquitetura geral.
- `CONSULTA/incb_esp32_idf.pdf` (Morais) — só sumário (16 pp).
- ESP-IDF stable docs (URL).
