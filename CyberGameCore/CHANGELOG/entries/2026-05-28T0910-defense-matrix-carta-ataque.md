---
data: 2026-05-28T09:10
tipo: add (codigo)
escopo: components/engine
trigger: usuario escolheu adiantar a "mecanica de carta NFC + matriz" enquanto refaz as imagens
commits:
  - (pendente)
---

# defense_matrix — matriz carta x ataque + lookup de UID

## Por que

Nucleo da mecanica de defesa: dada uma carta NFC e o ataque ativo, decidir o
resultado (CORRETO / INUTIL / AGRAVA). Logica pura, build-testavel, sem
depender de hardware nem das imagens novas.

## O que mudou (aditivo)

- `components/engine/include/defense_matrix.h` + `defense_matrix.c`:
  - `ataque_tipo_t` (DDOS, RANSOMWARE, PROPAGACAO_LATERAL) — novo enum.
  - `defesa_resultado_t` (CORRETO, INUTIL, AGRAVA).
  - `defense_resolve(carta, ataque)` — matriz de game_logic_decisions:
        |               | DDoS   | Ransom  | Propag
        | Isolamento    | inutil | inutil  | CORRETO
        | Backup        | AGRAVA | CORRETO | inutil
        | Balanceamento | CORRETO| AGRAVA  | inutil
  - `nfc_uid_to_carta(uid, len, &out)` — usa NFC_CARTAS de nfc_config.h
    (UIDs ainda placeholder 0xDEADBEEF — preencher com tags reais depois).
  - nomes p/ log/UI + `defense_matrix_selftest()`.
- `engine_init` chama `defense_matrix_selftest()` no boot -> imprime a matriz
  no log (validacao REMOTA pelo flash monitor; aux temporario, removivel).
- `engine/CMakeLists.txt`: +defense_matrix.c.

Build VERDE.

## Pendente / proximo

- **Integrar no fluxo do terminal** (FSM WAITING_CARD): hoje X/Y sao mock de
  carta certa/errada. Trocar pela leitura NFC real -> `nfc_uid_to_carta` ->
  `defense_resolve(carta, ataque_ativo)` -> efeito (CORRETO mitiga; INUTIL
  perde tempo; AGRAVA acelera VERMELHO_TIMER por VERMELHO_AGRAVADO_MULT_PCT).
  Depende do **sistema de ataques** (ataque ativo) que ainda nao existe.
- **UIDs reais** das 3 tags (usuario le com app/recovery) -> nfc_config.h.

## Links
- [[game_logic_decisions]] (matriz fonte), nfc_config.h
- docs/ai-rules (game logic)
