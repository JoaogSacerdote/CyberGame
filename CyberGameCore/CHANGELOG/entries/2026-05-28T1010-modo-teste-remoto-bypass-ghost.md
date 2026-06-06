---
data: 2026-05-28T10:10
tipo: add (codigo, DEV)
escopo: main, components/engine
trigger: usuario sem acesso fisico ao hardware (so flash + boot log) pediu bypass do PWR e um "jogador-fantasma" pra validar tudo pelo console
commits:
  - (pendente)
---

# Modo de teste remoto: bypass do PWR + jogador-fantasma

## Por que

O usuario so consegue gravar e ver o boot (acesso remoto, sem apertar botoes).
Dois bloqueios pra testar: (1) o latch do PMU exige segurar PWR pra bootar
NORMAL; (2) sem botoes nao da pra jogar. Solucao: uma variavel unica liga um
bypass + um jogador automatico que loga tudo.

## O que mudou

- `main/main.c`: nova variavel **`#define DEV_TEST_MODE 1`** (topo do arquivo,
  mudar so entre 1 e 0):
  - `#if DEV_TEST_MODE`: **bypassa o PMU** — `mode = PMU_BOOT_NORMAL` direto,
    pula o latch/abort e o `pmu_shutdown_monitor_task` (evita deep sleep com
    PWR flutuando). Pula tambem o `detect_dev_combo` (envolto em `#if !`).
  - chama `engine_set_test_mode(DEV_TEST_MODE)` antes de `engine_start`.
  - `0` = comportamento de producao (segura PWR; sem fantasma).
- `components/engine/engine.{h,c}`:
  - `engine_set_test_mode(bool)` — flag de runtime `s_test_mode`.
  - **`ghost_player_task`** (so roda se s_test_mode): espera 3s, forca
    `GAME_STATE_GAMEPLAY` (splash/menu usam button-peek, nao a queue), mantem
    `player_at_equipment`, e quando ha ataque ativo avanca o terminal
    injetando eventos na queue (Y -> A -> X carta correta) pra mitigar via a
    matriz. Loga `[GHOST] HH:MM | vidas | ataque` a cada minuto in-game e o
    desfecho no GAME_OVER (VITORIA/DERROTA), depois reinicia (A=retry).

Build VERDE (CyberGame.bin 0x95610, 85%% livre). PSRAM segue OFF (boot estavel).

## Como usar
- Testar remoto: `DEV_TEST_MODE 1` -> flash -> o boot mostra banner, init dos
  HALs, mount do SD, selftest da matriz, sim do loop, e depois o `[GHOST]`
  jogando sozinho. Valida FSM + threat + matriz + vitoria/derrota SEM tocar no
  aparelho.
- Producao / teste manual com hardware: `DEV_TEST_MODE 0`.

## Notas
- O fantasma chama `fsm_set_state` direto (pequena corrida com o engine_task,
  aceitavel em codigo de teste). Gated por DEV_TEST_MODE — nao vai pra producao.
- Com PSRAM off as salas nao renderizam (fundos grandes nao cabem), mas o loop
  e a logica rodam e sao logados — que e o ponto do fantasma.

## Links
- [[2026-05-28T0940-loop-de-jogo-ataques-vitoria-derrota]]
- [[hw_psram_boot_loop]], [[build_environment]]
