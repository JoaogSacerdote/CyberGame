---
tipo: moc
status: vigente
area: fsm
ultima-atualizacao: 2026-05-27
---

# MOC — FSM e Game Logic

> Mapa quando a pergunta envolve máquina de estados, eventos, tasks
> FreeRTOS do gameplay, ou loop principal do jogo.

## Estado em uma linha

FSM macro hierárquica (`GAME_STATE_*` com sub-estados `GAMEPLAY_SUB_*`)
mais sala (`GAMEPLAY_SALA_*`). 3 tasks FreeRTOS (UI/Game/Hardware) com
core affinity obrigatória. Queue própria de eventos (`fsm_event_t`) — sem
`esp_event` ainda.

## Estados macro

⭐ **Canonical**: seção 1 de [[diagramas-do-projeto]] +
`components/fsm/include/fsm_states.h`.

```
[*] → SPLASH → MENU → GAMEPLAY ⇄ PAUSE
                  ↓
            (10 estados, ver diagrama)
```

Estados vigentes:
- `GAME_STATE_SPLASH`
- `GAME_STATE_MENU`
- `GAME_STATE_GAMEPLAY`
- `GAME_STATE_PAUSE`
- `GAME_STATE_GAME_OVER`
- `GAME_STATE_RANKING_VIEW`
- `GAME_STATE_CREDITOS`

## Sub-estados dentro de GAMEPLAY

⭐ **Canonical**: seção 2 de [[diagramas-do-projeto]] +
`components/fsm/include/fsm_states.h` (enum `gameplay_substate_t`).

- `GAMEPLAY_SUB_EXPLORANDO`
- `GAMEPLAY_SUB_TERMINAL_ABERTO`
- `GAMEPLAY_SUB_WAITING_CARD`
- `GAMEPLAY_SUB_ACTION_LOCK`
- `GAMEPLAY_SUB_SYSTEM_DEPLOY`

Mais salas:
- `GAMEPLAY_SALA_RECEPCAO`
- `GAMEPLAY_SALA_EMPRESA`

## Eventos

Fonte: `components/fsm/include/fsm_events.h` (struct `fsm_event_t`).

Kinds atuais:
- `FSM_EVT_BUTTON` — payload: id + state
- `FSM_EVT_TICK` — payload: dt_ms

Periodicidade do tick: `ENGINE_TICK_PERIOD_MS` (ver `engine.c`).

## Tasks FreeRTOS

⭐ **Canonical**: seção 5 de [[diagramas-do-projeto]] +
`components/engine/engine.c`.

| task | core | prio | função |
|---|---|---|---|
| `engine_task` | Core 0 | 6 | consume queue de eventos, ticka FSM, sincroniza UI com state macro/sala |
| `button_reader_task` | qualquer | (herdada) | lê `button_hal_get_event` em loop bloqueante, despeja em queue do engine |
| LVGL task | Core 1 (config) | (LVGL default) | render LVGL via `lv_timer_handler` |

## Gating de periféricos pela FSM

| periférico | gating |
|---|---|
| NFC PN532 | `nfc_hal_start_scanning()` chamado em entrada de `GAMEPLAY_SUB_WAITING_CARD`; `stop_scanning` em saída. Sem isso, leitor fica ligado o tempo todo. |
| Display backlight | sempre ligado em estados visíveis (não implementado fade-on/off) |

## Plano de implementação ainda pendente

⭐ **Canonical**: [[plano-implementacao-game-logic]] (etapas A-G).

Estado atual aproximado (auditar antes de afirmar):
- A (esqueleto+menu) ✅ provavelmente feito
- B (feedback_hal: WS2812+buzzer) ❌ ADIADO pós-MVP (CONSULTA/A resolver.txt E1)
- C-G: em andamento, status real no código

## Relacionados

- [[MOC_game_design]] — regras, balanceamento, matriz ataque×carta
- [[MOC_firmware]] — engine/fsm são componentes ESP-IDF
- [[diagramas-do-projeto]] — 10 diagramas Mermaid, todos relacionados a FSM/tasks

## Pendências

Ver [[OPEN_QUESTIONS]] §fsm:
- Falta input service (G2): UI e FSM tocam HAL direto.
- Sem callback registration nos HALs de input (G3).
- Sem task watchdog em `engine_task` (G8).
- Sem `esp_event` (decisão consciente — usa queue própria).
