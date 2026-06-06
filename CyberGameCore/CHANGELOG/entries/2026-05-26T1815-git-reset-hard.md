---
date: 2026-05-26 18:15
type: revert
files:
  - components/hardware/button_hal.c
  - components/hardware/display_hal.c
  - components/hardware/nfc_hal.c
  - components/hardware/storage_hal.c
  - components/hardware/pmu.c
  - components/hardware/include/pmu.h
  - components/hardware/include/joystick_hal.h
  - components/engine/engine.c
  - components/engine/CMakeLists.txt
  - components/fsm/CMakeLists.txt
  - components/fsm/include/fsm_gameplay.h
  - components/fsm/include/fsm_states.h
  - components/ui/include/ui.h
  - components/ui/include/ui_internal.h
  - components/ui/ui_router.c
  - main/main.c
  - main/wokwi-project.txt
  - diagram.json
  - hardware/PINOUT
  - CMakeLists.txt
  - wokwi.toml
  - write_game.py
  - README.md
  - simulation/lv_port_pc_eclipse/* (40+ arquivos)
  - assets/dialogos/recepcionista.txt (DELETADO recuperado)
  - simulation/img/persoagem.png (DELETADO recuperado)
session: bring-up placa nova
---

# git reset --hard HEAD — DESCARTOU TRABALHO NÃO COMMITADO

## Motivo
Durante debug do boot loop, usuário pediu "voltar à última versão estável". Após confirmação ("descartar todas as modificações não commitadas, voltar ao HEAD = 0490971"), executei `git reset --hard HEAD`. 

Em tese o usuário entendeu o impacto (foi a opção que ele escolheu na pergunta), mas o resultado foi mais destrutivo do que ele provavelmente percebeu: descartou TODO o trabalho de pinout 2026-05-21 + fixes do refactor pré-profissional que estavam pendentes.

## Antes (resumido — não há como recuperar conteúdo exato)

Os arquivos listados continham:
- **Pinout novo 2026-05-21** em todos os HALs (button_hal, display_hal, nfc_hal, storage_hal, pmu)
  - Tabela completa preservada em memória: `project_pinout_revisao_2026_05_21.md`
- **Fixes** que faziam o refactor pré-profissional (commit 0490971) compilar
- Trabalho no simulador PC (`simulation/lv_port_pc_eclipse/*`)
- Mudanças em wokwi.toml, diagram.json, write_game.py
- Arquivos deletados: `assets/dialogos/recepcionista.txt`, `simulation/img/persoagem.png`

## Depois
Working tree limpo de modificações tracked. HEAD = `0490971`. Pinout dos HALs voltou ao antigo (botão A=11, START=GPIO 3 strap, NAND CS=3 strap, etc) que não bate com a placa nova nem com o schematic KiCad.

## Resultado
- ✅ Permitiu testar PSRAM disabled (objetivo imediato)
- ❌ Trabalho de pinout 2026-05-21 PERDIDO — vai ser refeito mecanicamente quando reabilitar PSRAM
- ❌ Bugs latentes do refactor 0490971 expostos (falta GAME_STATE_GAME_OVER, falta declarações de HUD/fsm helpers, falta REQUIRES gamestate em fsm) — consertados parcialmente em entradas seguintes
- ⚠️ Arquivos untracked **preservados**: `hardware/CyberGame_kicad/`, `hardware/esquematico/`, `pc_simulator/`, `screen_hud.c`, `screen_game_over.c`, `managed_components/`

## Lição
**Nunca mais hard-reset sem `git stash` ou commit-WIP antes.** Discussão no chat: a memória `project_pinout_revisao_2026_05_21` salvou a tabela do pinout, então é refazer mecânico em vez de inventar. Mas não dá pra recuperar mudanças cujo conteúdo não estava em memória.

## Links
- Memória: [[project_pinout_revisao_2026_05_21]]
- Próxima entrada: [[2026-05-26T1830-disable-psram]]
