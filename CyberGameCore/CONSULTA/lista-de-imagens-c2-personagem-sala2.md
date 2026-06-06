---
tags: [cybersec, assets, c2, personagem, salas, contrato]
projeto: CyberSec
status: parcialmente-entregue
data: 2026-05-13
relacionado: [[plano-implementacao-game-logic]], [[RESPOSTAS]], [[explicacao_sprints]]
---

# Contrato de assets — telas do MVP, por camada

> Este e o **contrato oficial** de assets graficos pro MVP. Estabelece o
> padrao de camadas, dimensoes e organizacao. Foi reescrito apos a entrega
> do primeiro lote pelo usuario em `CONSULTA/imagens_para_sprints/` em
> 2026-05-13.
>
> **Convencao adotada pelo projeto** (definida pelo usuario no
> `explicacao_sprints.txt`):
> - Numeracao no fim do nome do arquivo = ordem da camada (Z-index ascendente)
> - Sufixo `_NULL` = arquivo de orientacao, **nao renderizar** (so define area)
> - 1 arquivo PNG por camada por tela (480x320 sempre)

---

## 1. Padrao oficial de camadas (Z-order ascendente)

| # | Nome | Funcao | Atualizacao | Tem colisao? |
|---|---|---|---|---|
| `00` | `PISO_CAMADA_00.png` | Chao + paredes pintadas no fundo | Estatico, render 1 vez | Nao |
| `01` | (reservado) | Futuro: sombra do player ou outra camada baixa dinamica | — | Nao |
| `02` | `PAREDES_E_OBJETOS_02.png` | Moveis e obstaculos COM colisao | Estatico | **Sim** (extraida da imagem) |
| `03` | `COMPLEMENTO_SEMCOLISAO_PAREDES_E_OBJETOS_03.png` | Topo de objetos altos que ficam ACIMA do player no Z-order (player passa "por tras") | Estatico | Nao |
| `04+` | Elementos dinamicos com numero crescente: icones, NPCs em diferentes poses, alertas | Estado pode mudar (visivel/invisivel, frame) | Variavel | Geralmente nao |
| `_NULL` | `AREA_*_NULL.png` | Mascara de gatilho/area de interacao | Nao renderizar — usado so para extrair retangulo | N/A |

### Tipos de areas `_NULL` (codificados por cor do pixel pintado)

| Cor pintada | Significado | Acao no jogo |
|---|---|---|
| Marrom (#A04A40 aprox) | Area de interacao com NPC | Quando player entra, NPC vira pro player + icone `!` desaparece + B/A abre dialogo |
| Vermelho (#FF0000) | Area de gatilho de tarefa ou porta | Sair pra outra sala / iniciar tarefa verde/amarela |
| (futuro: outras cores conforme necessidade) | — | — |

---

## 2. Premissas tecnicas (firmes)

- **Display**: 480x320 paisagem RGB565 (calibrado: byte swap + BGR + R boost).
- **Todas as imagens de tela**: **480x320 px exato**. PNG.
  - Camada 00: PNG-24 (sem alpha) ou PNG-32 com alpha total — render como fundo
  - Camadas 02, 03, 04+: PNG-32 RGBA (alpha=0 nas partes vazias)
  - `_NULL`: PNG-32 RGBA (alpha=0 fora dos retangulos)
- **Personagem**: sprite-sheet 96x192, frames 32x48, grid 3 col × 4 lin.
- **Sem antialiasing** em nenhuma imagem — Nearest Neighbor sempre.

---

## 3. Texto via LVGL (sem imagem)

**Decisao 2026-05-13**: todo texto exibido no jogo e renderizado por `lv_label`
nativo do LVGL, **nao** como imagem. Economiza memoria, permite typewriter
animado, facilita localizacao.

### Fonte

- **Tipo**: fonte pixel art custom, gerada via `lv_font_conv`
- **Tamanhos**: 2 (12px corpo, 16px titulo)
- **Range Unicode**: ASCII 0x20-0x7F + Latin-1 Supplement 0xC0-0xFF (cobre acentos PT-BR `áàâãéêíóôõúçÁ…`)
- **Custo flash**: ~12 KB total (2 tamanhos)
- **Onde aplica**:
  - Caixa de dialogo (texto digitando palavra por palavra)
  - Hint "[A] >>  [B] Pular"
  - HUD (relogio do expediente, vidas, score)
  - Menu / Pause / Ranking / Cadastro de nick
  - Eventuais labels de notificacao

### Animacao typewriter

- Texto longo no dialogo: 1 caractere ou 1 palavra por frame de timer
- ~120ms por palavra (ajustavel)
- Botao A durante typewriter: completa instantaneamente
- Apos texto completo: aparece a caixa `[A] >>  [B] Pular`

### Botoes do dialogo (firmado)

| Botao | Acao |
|---|---|
| **A** | Avancar 1 linha (ou completar linha atual se em typewriter) |
| **B** | Pular dialogo inteiro (skip total) |
| **A segurado ≥500ms** | Fast-forward (opcional, polimento) |

Texto da caixa: **`[A] >>   [B] Pular`** (inverso do `explicacao_sprints.txt`,
ratificado em 2026-05-13).

---

## 4. Inventario do que ja foi entregue (lote 1, 2026-05-13)

### 4.1 TELA 02 — Recepcao (PRONTA, exceto sprite player)

Pasta: `CyberGameCore/CONSULTA/imagens_para_sprints/recepcao/`

| Arquivo | Dimensao | Status | Observacao |
|---|---|---|---|
| `PISO_CAMADA_00.png` | 480x320 | ✅ OK | Chao xadrez + tapete vermelho/dourado + parede superior |
| `PAREDES_E_OBJETOS_02.png` | 480x320 | ✅ OK | Prateleira, planta grande, monitor, microondas, mesa de trabalho (esq), balcao da recepcionista (dir) |
| `COMPLEMENTO_SEMCOLISAO_PAREDES_E_OBJETOS_03.png` | 480x320 | ✅ OK | 2 monitores em cima da mesa de trabalho, globo + planta em cima do balcao |
| `AREA_QUE_LIBERA_INTERACAO_COM_RECEPCIONISTA_NULL.png` | 480x320 | ✅ OK | Retangulo marrom no canto esq-meio (gatilho de dialogo) |
| `AREA_PARA_ACESSAR_ESCRITORIO_NULL.png` | 480x320 | ✅ OK | Retangulo vermelho no canto inf-esq (porta pra Empresa) |
| `ICONE_DE_NOTIFICACAO_RECEPCIONISTA_04.png` | 480x320 | ✅ OK | `!` azul piscante. Some apos primeira interacao |
| `RECEPCIONISTA_MECHENDO_NO_COMPUTADOR_SEM_INTERACAO_05.png` | 480x320 | ✅ OK | Pose padrao (de costas, mexendo no PC) |
| `RECEPCIONISTA_OLHANDO_PARA_JOGADOR_ENQUANTO_ACONTECE_DIALOGO_06.png` | 480x320 | ✅ OK | Pose quando player esta na area de interacao |
| `CAIXA_DE_DIALOGO_RECEPCIONISTA_07.png` | 480x320 | ✅ OK | Moldura preta + borda laranja + avatar embutido + seta `▶` |
| `CAIXA_DELIMITADORA_ONDE_PODE_APARECER_O_TEXTO_DO_DIALOGO_08.png` | 480x320 | ✅ OK | Caixa preta interna (espaco do texto) |
| ~~`CAIXA_PARA_PULAR_TEXTO_09.png`~~ | — | ❌ **NAO PRECISA** | Texto desenhado via LVGL `lv_label` (decisao 2026-05-13) |

### 4.2 TELA 01 — Empresa / Escritorio (PRONTA, exceto sprite player)

Pasta: `CyberGameCore/CONSULTA/imagens_para_sprints/empresa/`

| Arquivo | Dimensao | Status | Observacao |
|---|---|---|---|
| `PISO_CAMADA_00.png` | 480x320 | ✅ OK | Chao cinza com padrao de blocos + carpete sutil + portas |
| `PAREDES_E_OBJETOS_02.png` | 480x320 | ✅ OK | 2 mesas com monitores, 2 racks de servidor (canto dir), cadeiras, mesas auxiliares, cafeteira, mesa com PC (canto inf-dir) |
| `COMPLEMENTO_SEMCOLISAO_PAREDES_E_OBJETOS_03.png` | 480x320 | ✅ OK | 3 plantas, impressora, monitor extra (acima das mesas) |
| `AREA_DE_INTERACAO_PARA_TAREFA_VERDE_NULL.png` | 480x320 | ✅ OK | Retangulo vermelho no canto dir-inf (perto do PC de tarefa verde) |
| `AREA_DE_INTERACAO_DIREITA_DO_NPC_TI_NULL.png` | 480x320 | ✅ OK | Retangulo vermelho pequeno (ativa NPC_TI olhando pra direita) |
| `AREA_DE_INTERACAO_BAIXO_DO_NPC_TI_NULL.png` | 480x320 | ✅ OK | Retangulo vermelho pequeno (ativa NPC_TI olhando pra baixo) |
| `ICONE_TAREFA_VERDE_04.png` | 480x320 | ✅ OK | `!` verde no canto dir-inf, indica tarefa verde disponivel |
| `ICONE_TAREFA_AMARELA_05.png` | 480x320 | ✅ OK | `!` amarelo no canto dir-sup, indica tarefa amarela (NPC TI) |
| `NPC_TI_TAREFA_AMARELA_PARA_CIMA_06.png` | 480x320 | ✅ OK | NPC visto de frente (player se aproxima por baixo) |
| `NPC_TI_TAREFA_AMARELA_PARA_DIREITA_07.png` | 480x320 | ✅ OK | NPC de perfil direito (player se aproxima da esquerda) |
| `NPC_TI_TAREFA_AMARELA_PARA_BAIXO_08.png` | 480x320 | ✅ OK | NPC de costas — padrao quando player nao esta proximo |

### 4.3 Sprite do jogador

| Arquivo | Dimensao | Status |
|---|---|---|
| `Sprite_PLAYER.png` | **96 x 192** (3 col × 4 lin, frames 32x48 sem gap) | ✅ OK |

**Layout de frames confirmado**:

| Linha | Direcao | Coluna 0 | Coluna 1 | Coluna 2 |
|---|---|---|---|---|
| L0 | DOWN | passo esq | idle | passo dir |
| L1 | RIGHT | passo esq | idle | passo dir |
| L2 | LEFT | passo esq | idle | passo dir |
| L3 | UP | passo esq | idle | passo dir |

Frame em (col, lin) ocupa o retangulo `(col*32, lin*48, 32, 48)` no PNG.

Animacao walk: sequencia de colunas `[0, 1, 2, 1]` a ~8 fps enquanto joystick fletido. Parado → col=1.

---

## 5. Mapeamento das telas entregues para o codigo do jogo

| Nome no `explicacao_sprints.txt` | Nome no plano / RESPOSTAS.txt | Identificador no codigo |
|---|---|---|
| TELA 01 — Empresa | Sala 2 (Servidores + Escritorios) | `SALA_EMPRESA` |
| TELA 02 — Recepcao | Sala 1 (Recepcao) | `SALA_RECEPCAO` |

> Vou usar **os nomes do usuario** no codigo (`SALA_EMPRESA`, `SALA_RECEPCAO`)
> em vez de "Sala1/Sala2" pra evitar a inversao confusa.

---

## 6. O que ainda nao foi entregue (proximos lotes)

### Lote 2 — telas de gameplay restantes (Etapa D em diante)

- Sala 3 (Financeiro): PISO_00 + PAREDES_02 + COMPLEMENTO_03 + 1 area de servidor + areas `_NULL` da porta
- Sala 4 (RH): idem
- Overlays de estado de servidor sob ataque (alerta amarelo/vermelho/critico/destruido) — sprite-sheet ou 4 PNGs separados

### Lote 3 — telas finais (Etapa G)

- Splash inicial (decidiu refazer depois): logo + fundo + prompt LVGL
- Vitoria: bg + faixa
- Derrota: bg + faixa

### Lote 4 — adornos (opcional)

- NPCs adicionais nas salas (`secretario.png`, `npc_sala2.png`, `trabalhador_sala2.png` do colega — adiar pro polimento)

---

## 7. Pipeline tecnico (lado Claude — informativo)

1. **Leitura das areas `_NULL`**: vou processar pixel-a-pixel cada PNG `_NULL`,
   identificar o retangulo de pixels nao-transparentes, e extrair como
   `{x, y, w, h, tipo_por_cor}` numa tabela C estatica.
2. **Conversao PNG → array C** via `lv_img_conv`:
   - Camada 00 (piso): `LV_COLOR_FORMAT_RGB565` sem alpha — ~300 KB cada
   - Camadas 02/03/04+: `LV_COLOR_FORMAT_RGB565A8` com alpha — varia (5-50 KB)
   - `Sprite_PLAYER.png`: `LV_COLOR_FORMAT_RGB565A8` — ~10 KB
3. **Fonte custom**: gerada via `lv_font_conv` a partir de uma fonte pixel art
   livre (recomendacao: "Pixel Operator" ou "Press Start 2P", licencas livres),
   convertida em `lv_font_t` C estatica.
4. **Carregamento**: cada PNG vira `static const lv_image_dsc_t img_X` em
   `components/ui/screen_empresa.c` e `screen_recepcao.c`.
5. **Z-order LVGL**: parent unico (`screen_active`), filhos criados na ordem
   00 → 02 → 03 → 04+ → player. LVGL renderiza na ordem de criacao
   (primeiro criado = mais embaixo).
6. **Colisao**: tabela C de retangulos extraida automaticamente do PNG 02.
   Tambem extraio `_NULL` pra gatilhos.

---

## 8. Resumo executivo — onde estamos

**Etapa C2** (personagem + Sala 2 = TELA 01 Empresa, no padrao do usuario):

- ✅ Recepcao completa entregue (10 PNGs)
- ✅ Empresa completa entregue (11 PNGs)
- ✅ `Sprite_PLAYER.png` entregue (96x192)
- ⏳ Fonte pixel art PT-BR — vou pesquisar e gerar quando for codar
- ⏳ Texto dos dialogos do recepcionista — usuario vai escrever em PT-BR

**Nenhum bloqueio de asset.** Posso comecar a Etapa C2 agora.
