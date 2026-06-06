---
tipo: moc
status: vigente
area: game-design
ultima-atualizacao: 2026-05-27
---

# MOC — Game Design

> Mapa quando a pergunta envolve regras do jogo, salas, NPCs, mecânicas,
> scoring, narrativa, ou fundamentação pedagógica.

## Estado em uma linha

Jogo de defesa cibernética com expediente de 3 minutos. Jogador anda em
salas top-down, abre terminais, recebe ataques aleatórios, aplica cartas
NFC para defender. MVP: 3 ataques (DDoS, Ransomware, Propagação Lateral)
+ 3 cartas + matriz 3×3 de efeito.

## Matriz central

⭐ **Canonical**: [[matriz-reacao-ataques]] — ataque × severidade × sala ×
carta × pilar CIA, e matriz 3×3 de carta×ataque (correto / inútil / agrava).

## Salas

⭐ **Canonical**: seção 6 de [[diagramas-do-projeto]].

| sala | status | função |
|---|---|---|
| Recepção | implementada (assets) | tutorial + NPC introdutor |
| Escritório (Empresa) | implementada (assets) | gameplay principal |
| Servidores | ❌ falta arte | gameplay (Disponibilidade) |
| Financeiro | ❌ falta arte | gameplay (Integridade) |
| RH/Reunião | ❌ opcional | TBD |

Topologia: corredor central conectando todas (proposta, não confirmado).

## Mecânicas core

### Cartas NFC
- 3 cartas físicas (UIDs hardcoded em `nfc_config.h` — falta criar)
- Leitura via PN532 só em `GAMEPLAY_SUB_WAITING_CARD`
- Carta correta = mitiga; inútil = perde tempo; agrava = penalidade extra

### Tarefas
- **Verde** (calmas): 4 opções de senha (default — `A resolver.txt`)
- **Amarela** (médias): QTE Simon Says com 4 botões
- **Vermelha** (ataques críticos): exige carta NFC

### LEDs (3× WS2812)
⭐ **Canonical**: seção 9 de [[diagramas-do-projeto]] + memória `game_logic_decisions`.
- LED 1 (verde): tarefa verde pendente
- LED 2 (amarelo): tarefa amarela pendente; vira vermelho se ataque >33%
- LED 3 (vermelho): escalada do ataque (sólido → piscando sincronizado → caótico)

### Z-ordering LVGL
⭐ **Canonical**: seção 10 de [[diagramas-do-projeto]] (5 camadas, sem
buffers separados — só `lv_obj_move_foreground`).

## Scoring

⭐ **Canonical**: [[matriz-reacao-ataques]] §"Regras de balanceamento".

Constantes propostas (a viver em `components/gamestate/include/game_config.h`):
- `EXPEDIENTE_DURACAO_MS = 3 * 60 * 1000`
- `EVENTO_VERDE_INTERVALO_MS = 30000`
- `EVENTO_AMARELO_INTERVALO_MS = 60000`
- `EVENTO_VERMELHO_INTERVALO_MS = 90000`
- `ACTION_LOCK_MS = 1500`
- `SYSTEM_DEPLOY_MS = 4000`
- `SCORE_VERDE = 10`
- `SCORE_AMARELA = 20`
- `SCORE_VERMELHO_BASE = 50`
- `SCORE_VELOCIDADE_MAX = 50` (bônus reflexo)

## Pipeline de assets

⭐ **Canonical**: seção 8 de [[diagramas-do-projeto]].

`.aseprite` → exportar PNG → converter via `lvgl-image-converter` ou script
próprio → bin RGB565 → recovery USB CDC → NAND → `asset_store` →
`game_logic` → `lv_img_set_src` → LVGL → display.

Estado: leitura OK (asset_store); escrita não existe (extensão futura do
recovery: PUT/GET/LIST).

## Pedagogia

⭐ **Canonical**: `CONSULTA/Artigo.pdf` (fonte imutável).

Pilares cobertos (Stallings 2017 / Kim & Solomon 2018):
- Disponibilidade ← DDoS ← Balanceamento de Rede
- Integridade ← Ransomware ← Backup de Emergência
- Integridade ← Propagação Lateral ← Isolamento de Rede
- Confidencialidade ← Phishing (pós-MVP)

NFC materializa **autenticação por fator de posse** (Stallings 2017 §2.7).

## Decisões fechadas

⭐ **Canonical**: [[DECISION_LOG]].

Principais decisões MVP (consolidadas de RESPOSTAS.txt 2026-05-12):
- 3 tasks (não 4 como artigo): UI, Game, Hardware
- 3 LEDs WS2812 no mesmo barramento RMT
- 3 ataques no MVP (DDoS, Ransomware, Propagação); Phishing pós-MVP
- NAND-FS para assets + ranking
- Terminal aberto via botão Y
- Pause via START
- feedback_hal ADIADO (CONSULTA/A resolver.txt E1)

## Pendências de design

Ver [[OPEN_QUESTIONS]] §game-design:
- UIDs reais das 3 cartas NFC (bloqueador B1)
- Penalidade exata de carta AGRAVA
- Conteúdo do Dashboard do Analista (botão X)
- Tela final de vitória/derrota
- Número de salas (S5 opcional?)
- Cada sala associada a 1 pilar CIA?

## Pendências visuais / refactor

⭐ **Canonical**: memória `pending_visual_logic_refactor`.

Refazer pós-Fase 6: sprites escritório, Y-sorting, zonas por máscara
colorida, locais de tarefas, tela demissão, sprite player melhor.

## Relacionados

- [[MOC_fsm]] — implementação dos estados
- [[matriz-reacao-ataques]] — fonte primária
- [[diagramas-do-projeto]] — visualização completa
- `CONSULTA/Artigo.pdf` §3 — concepção do jogo
- `CONSULTA/lista-de-imagens-c2-personagem-sala2.md` — lista de assets
