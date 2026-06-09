# CHANGELOG — Índice

Ordenado do mais recente pro mais antigo.

## 2026-06-09 — Menu de servidor + tela DDoS + NFC + fix tarefa amarela

- [2026-06-09T1100 — Fix troca de painel enquanto segura HD](entries/2026-06-09T1100-fix-amarela-panel-switch.md) — `fix` — removido `!s_holding` da condição de troca de painel; ESTOQUE↔BAIA agora funciona com HD em mãos
- [2026-06-09T1000 — Menu de servidor + tela DDoS + NFC integrado](entries/2026-06-09T1000-menu-servidor-ddos-nfc.md) — `add+edit` — menu "[ SERVIDOR ]" em ambos os servidores; tela WEB com detecção DDoS; NFC poll no engine_task; fallback UID → CARTA_BALANCEAMENTO

## 2026-06-08 — Telas de tarefa + wiring empresa

- [2026-06-08T1800 — Wiring das telas de tarefa no screen_empresa](entries/2026-06-08T1800-wiring-tarefas-empresa.md) — `edit` — tarefa verde e amarela conectadas ao empresa_tick; placeholder removido; header screen_tarefa_amarela.h criado

## 2026-05-28 — Feedback HAL (LED + buzzer) + microSD + PSRAM + layout

- [2026-05-28T1110 — Tuning do pacing + selftest write/read do SD](entries/2026-05-28T1110-pacing-tune-sd-selftest.md) — `edit+add` — vermelho 30s/1o aos 10s (loop perdivel: SIM A=VITORIA, B=DERROTA); sd_hal_selftest grava+le arquivo
- [2026-05-28T1010 — Modo teste remoto: bypass PWR + ghost player](entries/2026-05-28T1010-modo-teste-remoto-bypass-ghost.md) — `add` — DEV_TEST_MODE em main.c (1/0): bypassa PMU + jogador-fantasma que joga e loga sozinho
- [2026-05-28T0940 — Loop de jogo (ataques + vitoria/derrota)](entries/2026-05-28T0940-loop-de-jogo-ataques-vitoria-derrota.md) — `add+edit` — threat + relogio rodando + win/lose + matriz no terminal + sim selftest; build verde. Pacing precisa tuning
- [2026-05-28T0910 — defense_matrix (carta x ataque)](entries/2026-05-28T0910-defense-matrix-carta-ataque.md) — `add` — enums ataque/resultado + matriz + lookup UID + selftest no boot; build verde; integracao no terminal pendente
- [2026-05-28T0840 — Loader runtime do room_layout](entries/2026-05-28T0840-loader-runtime-room-layout.md) — `add` — room_layout_spawn (pool+asset+y_sort, tolerante sem PSRAM); build verde; wiring nas telas pendente
- [2026-05-28T0820 — Pipeline de layout: ENTIDADES + POSICOES](entries/2026-05-28T0820-pipeline-layout-entidades-posicoes.md) — `add` — autoria em texto (catalogo+posicoes) -> room_layout_gen.c; build verde; loader runtime pendente

- [2026-05-28T0730 — Projeto SD-only + PSRAM revertida](entries/2026-05-28T0730-projeto-sd-only-revert-psram.md) — `edit+revert` — consolida em SD-only (NAND preservada); PSRAM@40 tb deu boot loop -> off; imagens dependem de PSRAM (hardware)
- [2026-05-28T0610 — Reabilita PSRAM Octal 80MHz (REVERTIDO)](entries/2026-05-28T0610-reabilita-psram-octal.md) — `edit+revert` — reproduziu o boot loop; revertido no mesmo dia; PSRAM segue off ate testar 40MHz/Quad
- [2026-05-28T0540 — Fase 2: assets lidos do microSD](entries/2026-05-28T0540-fase2-assets-na-sd.md) — `edit+add` — asset_loader/dialog_loader leem /sd/assets/<type>_<id>.bin; gerador build_sd_assets.py; fix registry dialogo
- [2026-05-28T0230 — Deteccao do microSD + NAND desativada](entries/2026-05-28T0230-sd-card-detect-disable-nand.md) — `add+edit` — sd_hal (SDSPI+FATFS, CS=47); NAND off no boot; Fase 1 (so detectar)
- [2026-05-28T0158 — feedback_hal: ws2812_hal + buzzer_hal](entries/2026-05-28T0158-feedback-hal-ws2812-buzzer.md) — `add` — Etapa B reativada: WS2812 (GPIO 8, led_strip/RMT) + buzzer (GPIO 21, LEDC TIMER_1)

## 2026-05-27 — Refactor de arquitetura + reorganização do vault + robustez

- [2026-05-27T1126 — Pacote robustez G6+G8+G9](entries/2026-05-27T1126-pacote-robustez-G6-G8-G9.md) — `feat (3 commits)` — asserts nos HALs + task WDT no engine + banner de boot
- [2026-05-27T1054 — Vault reorganizado para uso por agente](entries/2026-05-27T1054-vault-reorganizacao-agente.md) — `refactor (vault)` — criado `_AGENT/`, frontmatter padronizado, históricas arquivadas, pastas vazias removidas
- [2026-05-27T1039 — Centralização de pinos em board_pins.h](entries/2026-05-27T1039-board-pins-centralizacao.md) — `refactor` — auditoria pós-estudo Beningo/Elecia/ESP-IDF; build verde

## 2026-05-26 — Bring-up placa nova

- [2026-05-26T2200 — Inversão MOSI ↔ MISO (fix do swap)](entries/2026-05-26T2200-fix-mosi-miso-swap.md) — `edit` — provavelmente a causa da tela branca; corrige tela + NAND
- [2026-05-26T2110 — NAND compartilha SPI2 com tela](entries/2026-05-26T2110-nand-share-spi-tela.md) — `edit` — resolve conflito da T2100 movendo NAND pros mesmos pinos
- [2026-05-26T2100 — Re-pinagem da tela (v2)](entries/2026-05-26T2100-pinout-tela-v2.md) — `edit` — CS=5, RST=6, DC=7, MOSI=18, SCK=16, LED=17, MISO=15
- [2026-05-26T2015 — Fixes dos findings da auditoria](entries/2026-05-26T2015-fix-audit-findings.md) — `edit` — SPIRAM fallback + comentários GPIO atualizados
- [2026-05-26T2000 — Script de auditoria criado](entries/2026-05-26T2000-audit-routine.md) — `add` — tools/audit-project.ps1, busca erros comuns sem compilar
- [2026-05-26T1945 — MODO BRING-UP completo (todos HALs tolerantes)](entries/2026-05-26T1945-bring-up-mode-completo.md) — `edit` — todos os hardware-inits viram `LOGW` + continua; idle-loops removidos
- [2026-05-26T1930 — Reaplicação do pinout novo](entries/2026-05-26T1930-reaplica-pinout.md) — `edit` — botões/display/NAND/NFC pra bater com placa nova; NFC SDA agora é 39 (não 38)
- [2026-05-26T1900 — NFC tolerante em main.c](entries/2026-05-26T1900-nfc-tolerante.md) — `edit` — bench sem NFC físico
- [2026-05-26T1845 — Fixes de build pós-reset (HUD, fsm helpers)](entries/2026-05-26T1845-fixes-build-pos-reset.md) — `edit` — HEAD estava com código não-compilável
- [2026-05-26T1830 — Desligamento de PSRAM (resolveu boot loop)](entries/2026-05-26T1830-disable-psram.md) — `edit` — PSRAM Octal instável neste módulo
- [2026-05-26T1815 — git reset --hard HEAD (PERDA DE TRABALHO)](entries/2026-05-26T1815-git-reset-hard.md) — `revert` — descartou 40+ arquivos de trabalho não commitado
- [2026-05-26T1700 — COM port 19 → 17 em settings.json](entries/2026-05-26T1700-com-port-17.md) — `edit` — placa nova em COM17
