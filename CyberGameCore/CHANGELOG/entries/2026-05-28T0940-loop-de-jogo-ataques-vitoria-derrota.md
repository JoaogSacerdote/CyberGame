---
data: 2026-05-28T09:40
tipo: add + edit (codigo)
escopo: components/engine, components/fsm, components/gamestate
trigger: usuario escolheu adiantar o "loop de jogo (ataques + vitoria/derrota)"
commits:
  - (pendente)
---

# Loop de jogo: relogio rodando + ataques + vitoria/derrota

## Por que

O loop era so esqueleto: `gamestate_tick()` NUNCA era chamado (relogio
parado), nao havia ataques, nem condicao de fim. Esta entrada acende o loop.

## O que mudou

- `components/engine/threat.{h,c}` (novo) — sistema de ataque vermelho:
  spawn por intervalo (EVENTO_VERMELHO_INTERVALO_MS), timer ate destruir o
  setor (VERMELHO_TIMER_MS), `threat_mitigate(carta)` via matriz
  (CORRETO limpa / AGRAVA acelera VERMELHO_AGRAVADO_MULT_PCT / INUTIL nada),
  `threat_carta_correta()`, getters. 1 ataque por vez.
- `components/gamestate`: + `game_result_t` (EM_ANDAMENTO/VITORIA/DERROTA) +
  set/get. (init/reset ja zeravam relogio/vidas.)
- `components/fsm`: + hook `fsm_set_card_resolver()` (callback int(int)) pra
  desacoplar a mitigacao do engine (sem dep circular). WAITING_CARD agora
  chama o resolver (X=carta correta mock, Y=errada) em vez de mexer em vidas
  direto.
- `components/engine/engine.c`:
  - `gameplay_model_tick()` (chamado no tick durante GAMEPLAY): `gamestate_tick`
    + `threat_tick`; ataque expira -> `gamestate_perder_vida`; vidas==0 ->
    RESULT_DERROTA -> GAME_OVER; relogio>=18:00 -> RESULT_VITORIA -> GAME_OVER.
  - `engine_card_resolver()` registrado na FSM (aplica a matriz no ataque ativo).
  - reset de relogio+ataques na entrada FRESCA em GAMEPLAY (nao no resume de PAUSE).
  - `engine_init`: gamestate_init + threat_init + registra resolver +
    `gameplay_sim_selftest()`.

Build VERDE (CyberGame.bin 0xa55f0).

## Validacao REMOTA (boot log)

`engine_init` roda no boot:
- `defense_matrix_selftest()` -> imprime a matriz inteira.
- `gameplay_sim_selftest()` -> roda 2 simulacoes rapidas do expediente:
  - **A (mitiga sempre)**: deve sobreviver ate 18:00 -> VITORIA, perdas=0.
  - **B (nunca mitiga)**: loga perdas/vidas/resultado reais.

So precisa gravar + colar o boot — sem precisar jogar.

## BALANCEAMENTO (achado importante)

Com a config atual (EVENTO_VERMELHO_INTERVALO_MS=90s, VERMELHO_TIMER_MS=20s,
EXPEDIENTE=180s, VIDAS=3): cabem ~1 ataque vermelho por run -> com 3 vidas,
**a run e praticamente improvavel de perder** (cenario B tende a VITORIA).
Pacing precisa de tuning pra ficar tenso/perdivel (ex.: intervalo menor, 1o
ataque mais cedo, ou menos vidas). Sao numeros de game_config.h ("ajustaveis
sem mexer na FSM") — decisao de game design do usuario.

## Pendente
- Tuning do pacing (game_config) — decidir com o usuario.
- Integrar leitura NFC real (substituir o mock X/Y) — depende de UIDs reais.
- LED/buzzer no ataque (frente feedback_hal) + HUD (relogio/vidas) + tela de
  vitoria distinta da derrota (hoje ambas vao pra GAME_OVER; usar gamestate_get_result).

## Links
- [[2026-05-28T0910-defense-matrix-carta-ataque]], [[game_logic_decisions]]
