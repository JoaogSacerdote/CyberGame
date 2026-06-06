---
tipo: agent-reference
status: vigente
area: meta
ultima-atualizacao: 2026-05-27
fonte-detalhada: CONSULTA/A resolver.txt
---

# OPEN_QUESTIONS — Pendências consolidadas

> Síntese navegável das pendências do projeto. Versão detalhada e raw
> está em `CONSULTA/A resolver.txt`. Quando resolver um item: marcar `[x]`
> AQUI e lá. Quando criar item novo: criar AQUI e propagar pra lá.
> Itens são agrupados por **quem é o gating** e **impacto na próxima etapa**.

## 🔴 BLOQUEADORES (sem isto, etapa atual não anda)

### B1. UIDs reais das 3 cartas NFC físicas
- **Gating**: usuário precisa ler as 3 tags (com `recovery` mode + PN532, ou app celular)
- **Sem isso**: `nfc_config.h` fica com placeholder → não dá pra validar carta correta vs errada na Etapa C do [[plano-implementacao-game-logic]]
- **Onde aplicar**: criar `components/engine/include/nfc_config.h` (já listado mas vazio)
- Raw: `A resolver.txt` T3

### B2. Recovery USB CDC precisa expandir PING/PONG → PUT/GET/LIST
- **Gating**: ninguém implementou
- **Sem isso**: não dá pra carregar assets na NAND, bloqueia G (telas finais com imagens)
- **Onde aplicar**: `components/recovery/recovery.c`
- Raw: `plano-implementacao-game-logic.md` Bloqueador B2

### B3. Arte de Sala 3 (Servidores), Sala 4 (Financeiro), Sala 5/RH
- **Gating**: arte (Aseprite) — fora do escopo de Claude
- **Sem isso**: MVP roda só em Recepção + Empresa
- Raw: `plano-implementacao-game-logic.md` Bloqueador B3

## 🟡 ARQUITETURA / FIRMWARE (decisões de Claude pendentes ou gaps técnicos)

Gaps identificados na auditoria de 2026-05-27 contra Beningo / Elecia / ESP-IDF.
Ordem por prioridade (G1 mais urgente):

### G1. Pinout centralizado ✅ FEITO em 2026-05-27
- [[2026-05-27T1039-board-pins-centralizacao]]

### G5. docs/ai-rules/ ✅ FEITO em 2026-05-27 (parcial)
- Commit a0c3e4a — 6 arquivos + README versionados no repo público
- Doxygen ainda não obrigatório (ver §06-coding-standard.md)

### G6. Asserts nos HALs ✅ FEITO em 2026-05-27
- Commit 8d25c48 — display/button/nfc/storage com `assert()` nas APIs públicas

### G8. Task watchdog no engine_task ✅ FEITO em 2026-05-27
- Commit 2f87793 — `esp_task_wdt_add(NULL)` + reset no loop
- sdkconfig já tinha WDT habilitado (timeout 5s) e BOD level 7 (max)

### G9. firmware_version.h + banner no boot ✅ FEITO em 2026-05-27
- Commit af739e1 — `main/version.h` (0.1.0) + banner com IDF_VER + reset reason

### G2. Input service abstraindo button/joystick/nfc
- **Sintoma**: UI screens, FSM e engine importam `button_hal.h` direto
- **Arquivos que violam**: `components/fsm/fsm.c:5`, `components/ui/screen_menu.c:7`, etc.
- **Impacto**: difícil trocar input physical (joystick → D-pad) sem cascata de edits
- **Plano**: criar `components/input/` que abstrai; UI/FSM/engine dependem só dele

### G3. Callback registration nos HALs de input
- **Sintoma**: `button_hal.h` só expõe `get_event` (bloqueante) e `peek`; `display_hal` tem `register_trans_done_cb` mas input HALs não
- **Plano**: adicionar `button_hal_register_cb(button_id_t, cb)` (Beningo cap 6 step 3)

### G4. Init com config table (Beningo tip #9)
- **Sintoma**: `*_init(void)` sem parâmetros — pinout, debounce, deadzone hardcoded
- **Plano**: `*_init(const *_config_t *cfg)` — combinar com G1 (board header)

### G5. Coding standard + doxygen documentados
- **Sintoma**: não existe `docs/CODING_STANDARD.md` no repo; comentários PT sem `@param/@return`
- **Plano**: criar `docs/ai-rules/` com 6 arquivos (architecture, fsm, event, hal, freertos, coding) — sugerido pelo usuário em rodada anterior

### G6. Design by Contract / asserts nos HALs
- **Sintoma**: nenhum `assert()` validando pré-condições (ex.: `display_hal_draw_bitmap` não checa coords)
- **Plano**: adicionar asserts onde input vem do "lado de fora"

### G7. (resolvido — placeholder)

### G8. Task watchdog + brownout detector
- **Sintoma**: `engine_task` é loop infinito sem WDT-feed visível
- **Plano**: `esp_task_wdt_init` + `esp_task_wdt_add` na engine_task

### G9. firmware_version.h imprimindo no boot
- **Sintoma**: sem versionamento. Elecia cap 2 "Version Your Code"
- **Plano**: `components/main/version.h` com A.B.C, ESP_LOGI no boot

### G10. esp_event para pub/sub (DECISÃO consciente: NÃO)
- FSM usa queue própria (`fsm_event_t`). Manter — só registrar como decisão.

## 🟢 DISPLAY / CALIBRAÇÃO

### D1. Cinzas com leve tonalidade errada
- **Decidido**: aceitar como está no MVP (defaults temporários OK)
- **Solução futura**: gamma não-linear (R^0.7) ou boost só em cores quentes
- Raw: `A resolver.txt` D1

## 🎮 GAME DESIGN (mecânicas)

### A2. Penalidade exata de carta AGRAVA
- **Default temporário**: tempo restante do timer cai pela metade (`VERMELHO_AGRAVADO_MULT = 0.5`)
- Raw: `A resolver.txt` A2

### A3. Penalidade de carta INÚTIL
- **Default temporário**: nenhuma além do tempo perdido
- Raw: `A resolver.txt` A3

### A5. Mecânica tarefa VERDE (troca de senha)
- **Default temporário**: 4 opções na tela (3 fracas + 1 forte), joystick navega, A confirma
- Raw: `A resolver.txt` A5

### A6. Mecânica tarefa AMARELA (Simon Says)
- **Default temporário**: 4 botões, 1s exibição, 8s para repetir, falha imediata em erro
- Raw: `A resolver.txt` A6

## 🖼️ UI / TELAS

### U1. Dashboard do Analista (botão X) — layout exato
### U2. Tela de Splash — logo? Animação?
### U3. Tela de Vitória ("Promovido")
### U4. Tela de Derrota ("Demissão")
### U5. QR Code da pesquisa pedagógica
### U6. Tela de cadastro de nick — defaults: 6 chars, grid 5x6 A-Z

Todas com defaults temporários em `A resolver.txt` §"UI E TELAS".

## 📝 NARRATIVA (texto)

### N1. Diálogo do NPC recepcionista
- **Quem escreve**: equipe (não Claude por padrão)

### N2. Nome do protagonista
- **Default**: "Analista"

### N3. Texto das telas de tarefa
- **Quem escreve**: equipe

## 🎨 ASSETS

### G1-G7 (não confundir com gaps de arquitetura G1-G10 acima)
Lista em `A resolver.txt` §"ARTE E ASSETS". Todos pendentes de criação por
equipe de arte. Sem assets, telas usam placeholders.

## ⚙️ TÉCNICAS

### T1. Cooldown NFC: default 1s
### T2. Sem save mid-game (sessão curta de 3 min)
### T4. Pipeline Aseprite → bin: lvgl-image-converter (default)
### T5. **LittleFS** (decidido) para NAND-FS — usar `esp_littlefs` quando implementar

## ♿ ACESSIBILIDADE

### X1. Daltonicos: usar símbolos textuais além das cores (default)
### X2. Público-alvo: médio/superior (default)

## 🔮 WISHLIST (pós-MVP)

W1. Phishing (4º ataque, substituição educacional)
W2. Modo cooperativo 2 cartas (2FA pedagógico)
W3. Easter eggs
W4. Attract mode após 30s idle
W5. Múltiplos cenários (Hospital, Banco)
W6. Missões diárias / briefing dinâmico
W7. Estatísticas de rede animadas
W8. Música de fundo (NÃO no MVP)

## Como resolver um item

1. Marcar como `[x]` AQUI E em `CONSULTA/A resolver.txt`.
2. Se altera valor de `game_config.h` ou `nfc_config.h`: editar código.
3. Se altera arquitetura/contrato: atualizar memória relevante + nota canonical.
4. Adicionar entrada em [[DECISION_LOG]] se for decisão fechada com motivo.
