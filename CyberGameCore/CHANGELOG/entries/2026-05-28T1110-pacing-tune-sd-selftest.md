---
data: 2026-05-28T11:10
tipo: edit + add (codigo)
escopo: components/gamestate, components/engine, components/hardware, main
trigger: validacao remota do loop (sim B nao perdia) + pedido de teste write/read do cartao SD
commits:
  - (pendente)
---

# Tuning do pacing (loop perdivel) + selftest write/read do SD

## Por que

1. A simulacao do loop no boot mostrou que com a config antiga (vermelho a
   cada 90s) so cabia ~1 ataque por partida -> com 3 vidas a run era
   IMPOSSIVEL de perder (SIM B dava VITORIA). Pacing precisava ficar tenso.
2. O cartao SD parou de montar (timeout 0x107) — usuario pediu um teste que
   grave e leia um arquivo pra validar o cartao de ponta a ponta.

## O que mudou

### Pacing (game_config.h + threat.c)
- `EVENTO_VERMELHO_INTERVALO_MS`: 90s -> **30s** (entre ataques).
- novo `EVENTO_VERMELHO_PRIMEIRO_MS` = **10s** (1o ataque da run vem cedo);
  `threat_init` usa ele no spawn inicial.
- Resultado validado no boot (HW): **SIM A (mitiga sempre) = VITORIA**
  (mitigados=6, perdas=0); **SIM B (nunca mitiga) = DERROTA** (perdas=3,
  vidas=0 por volta de 15:13). Loop agora ganhavel E perdivel.

### SD write/read selftest (sd_hal + main)
- `sd_hal_selftest()` (novo): cria `/sd/_selftest.txt`, escreve uma string
  conhecida, fecha, reabre, le com fgets, compara, loga PASSOU/FALHOU. Valida
  o caminho de escrita+leitura (nao so o mount). Pre-cond: cartao montado.
- `main.c`: chama `sd_hal_selftest()` apos `sd_hal_list_root()` quando o mount
  da certo.

Build VERDE (CyberGame.bin 0x95aa0, 85%% livre).

## Estado do SD (HW)

O cartao **nao esta montando** agora (`sdmmc_card_init failed 0x107` TIMEOUT),
apesar de ter montado antes (SU02G 2GB) com o mesmo codigo. Causa = conexao/
encaixe (cartao fora do slot ou fio MISO=18 / SD_CS=47 solto) — hardware, nao
codigo. O selftest so roda quando o mount voltar a funcionar (usuario reencaixa
no fim do dia).

## Validado remotamente neste boot
bypass do PWR (DEV_TEST_MODE), matriz carta x ataque, loop (VITORIA e DERROTA
nas simulacoes), jogador-fantasma. Tudo pelo log de boot, sem tocar no aparelho.

## Links
- [[2026-05-28T0940-loop-de-jogo-ataques-vitoria-derrota]]
- [[2026-05-28T1010-modo-teste-remoto-bypass-ghost]]
- [[2026-05-28T0230-sd-card-detect-disable-nand]]
