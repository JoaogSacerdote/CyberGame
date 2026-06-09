# Análise Completa do Ciclo de Vida do Jogo
**CyberGame — Revisão total 2026-06-08 | Auditoria completa boot → desligamento**

> Este documento cobre **tudo**: FSM, subsistemas, o que está funcional, o que é stub e o que precisa ser implementado para o jogo ser jogável.

---

## ÍNDICE

1. [Ciclo de vida completo — visão geral](#1-ciclo-de-vida-completo)
2. [Boot e inicialização](#2-boot-e-inicialização)
3. [FSM — estrutura em 3 dimensões](#3-fsm---estrutura-em-3-dimensões)
4. [Diagrama de transições macro](#4-diagrama-de-transições-macro)
5. [Sub-FSM do Gameplay](#5-sub-fsm-do-gameplay)
6. [Cada tela em detalhe](#6-cada-tela-em-detalhe)
7. [Subsistemas paralelos](#7-subsistemas-paralelos)
8. [Tabela de status — o que está implementado](#8-tabela-de-status)
9. [Problemas e inconsistências identificados](#9-problemas-e-inconsistências)
10. [O que falta para o jogo ser jogável](#10-o-que-falta-para-ser-jogável)

---

## 1. Ciclo de Vida Completo

```
[LIGAR O CONSOLE]
      │
      ▼
  main.c: boot banner → engine_init() → engine_start()
      │
      ▼
┌─── SPLASH ─────────────────────────────────────────────────┐
│  Mostra logo. [A] → MENU                                    │
└────────────────────────────────────────────────────────────┘
      │ [A]
      ▼
┌─── MENU ───────────────────────────────────────────────────┐
│  3 opções: "Iniciar Expediente" / "Ranking" / "Sobre"       │
│  LEDs: arco-íris verde/amarelo/vermelho (decorativo)        │
│  [A] "Iniciar" → GAMEPLAY                                   │
│  [B] → SPLASH                                               │
│  [A] "Ranking" → ESP_LOGI "TODO" (nada acontece)           │
│  [A] "Sobre"   → ESP_LOGI "TODO" (nada acontece)           │
└────────────────────────────────────────────────────────────┘
      │ [A] "Iniciar"
      ▼
┌─── GAMEPLAY / RECEPCAO ────────────────────────────────────┐
│  Spawn: pivot (161,83)                                      │
│  NPC recepcionista: fala ao pressionar [A] perto            │
│  Diálogo: typewriter, [A] avança, [B] pula                  │
│  [A] na porta do escritório? → entra (sem verificação)      │
│  [START] → PAUSE                                            │
└────────────────────────────────────────────────────────────┘
      │ [toca AREA_PORTA_EMPRESA]
      ▼
┌─── GAMEPLAY / EMPRESA ─────────────────────────────────────┐
│  Spawn: pivot (24,165)                                      │
│  LEDs: verde (PC) + âmbar (rack) quando sem ataque          │
│  [A] perto do PC (AREA_TAREFA_VERDE)  → tarefa verde overlay│
│  [A] perto do rack (AREA_TAREFA_AMARELA) → tarefa amarela   │
│  [Y] perto de equip (AREA_NFC) → terminal NFC (sub-FSM)    │
│  [START] → PAUSE                                            │
│  [toca AREA_PORTA_RECEPCAO] → volta RECEPCAO                │
└────────────────────────────────────────────────────────────┘
      │ [18:00 ou vidas=0]
      ▼
┌─── GAME OVER ──────────────────────────────────────────────┐
│  PROMOVIDO (verde) ou DEMITIDO (vermelho)                   │
│  [A] → retry (GAMEPLAY, resetando tudo)                     │
│  [B] → MENU                                                 │
│  [30s idle] → SPLASH                                        │
└────────────────────────────────────────────────────────────┘
```

A qualquer momento em GAMEPLAY:
```
      [START] → PAUSE → [START] retoma / [B] → MENU
```

---

## 2. Boot e Inicialização

### Sequência exata (main.c → engine.c)

```
main()
  └─ app_main()
       ├─ (boot banner: versão + IDF + reset reason)
       └─ engine_start()
             ├─ engine_init()
             │     ├─ xQueueCreate(32, fsm_event_t)   — fila de eventos
             │     ├─ fsm_init()                        — estado = SPLASH
             │     ├─ entity_pool_init()
             │     ├─ y_sort_init()
             │     ├─ gamestate_init()                  — relógio, vidas, resultado
             │     ├─ threat_init()                     — sem ameaças ativas
             │     ├─ fsm_set_card_resolver()           — liga FSM ao engine
             │     ├─ defense_matrix_selftest()         — loga matriz no boot
             │     ├─ gameplay_sim_selftest()           — simula 2 runs no boot
             │     ├─ ui_init()                         — LVGL init
             │     └─ ui_show_splash()                  — abre tela inicial
             │
             └─ engine_start()
                   ├─ xTaskCreate(button_reader_task)   — lê botões → fila
                   ├─ xTaskCreatePinnedToCore(engine_task, Core 0)
                   └─ (se test_mode) xTaskCreate(ghost_player_task)
```

### Estado inicial das variáveis FSM

| Variável | Valor inicial |
|---|---|
| `s_current` | `GAME_STATE_SPLASH` |
| `s_sub` | `GAMEPLAY_SUB_EXPLORANDO` |
| `s_sala` | `GAMEPLAY_SALA_RECEPCAO` |
| `s_player_at_equipment` | `false` |
| `s_phase_ms` | `0` |

### Estado inicial do Gamestate

| Variável | Valor inicial |
|---|---|
| `s_elapsed_ms` | `0` |
| `s_vidas` | `3` (VIDAS_INICIAIS) |
| `s_result` | `RESULT_EM_ANDAMENTO` |
| `s_expediente_ativo` | `false` |

---

## 3. FSM — Estrutura em 3 Dimensões

A FSM tem **3 dimensões simultâneas** em memória:

```
┌──────────────────────────────────────────────┐
│  ESTADO MACRO  (game_state_t)                │
│  "Em que tela/modo estamos?"                 │
│                                              │
│  SPLASH → MENU → GAMEPLAY → PAUSE            │
│                          └→ GAME_OVER        │
│       RANKING_VIEW, CREDITOS (definidos,     │
│       nunca usados)                          │
├──────────────────────────────────────────────┤
│  SUB-ESTADO DO GAMEPLAY (gameplay_substate_t)│
│  (só existe dentro de GAMEPLAY)              │
│                                              │
│  EXPLORANDO → TERMINAL_ABERTO →              │
│  WAITING_CARD → ACTION_LOCK →                │
│  SYSTEM_DEPLOY → EXPLORANDO                 │
├──────────────────────────────────────────────┤
│  SALA  (gameplay_sala_t)                     │
│  (só existe dentro de GAMEPLAY)              │
│                                              │
│  RECEPCAO  ↔  EMPRESA                        │
└──────────────────────────────────────────────┘
```

### Como os eventos chegam na FSM

```
button_reader_task ──[FSM_EVT_BUTTON]──► fila ──► fsm_handle_event()
engine_task ─────────[FSM_EVT_TICK]────► fila ──► fsm_handle_event()

screens (peek direto) ────────────────────────► fsm_set_state()
                                                 fsm_set_gameplay_sala()
                                                 fsm_set_player_at_equipment()
```

**Dois caminhos paralelos** — a fila processa INPUT global (botões START/A/B/X/Y para FSM macro + NFC sub-FSM); as telas usam `button_hal_peek()` diretamente para INPUT local (movimento, diálogo, tarefas verdes/amarelas).

---

## 4. Diagrama de Transições Macro

```
              [boot]
                │
                ▼
          ┌──────────┐
          │  SPLASH  │──── [A] ─────────────────────►┌──────────┐
          └──────────┘                                │   MENU   │
                ▲                                     └──────────┘
                │[30s]◄── GAME_OVER idle timeout            │
                │                                           │ [A] "Iniciar"
                │◄───────────────────────────── [B] ────────┤
                                                            │
                                                            ▼
          ┌─────────────────┐          ┌───────────────────────────────┐
          │   GAME_OVER     │◄─────────│           GAMEPLAY            │
          │  PROMOVIDO /    │ vidas=0  │  sala = RECEPCAO ou EMPRESA   │
          │  DEMITIDO       │ ou 18:00 │  sub  = EXPLORANDO (+ outros) │
          └─────────────────┘          └───────────────────────────────┘
                │                                    │ [START]
                │[A] retry                           ▼
                │                          ┌─────────────────┐
                └─────────────────────────►│     PAUSE       │
                                           └─────────────────┘
                                                    │ [START] retoma
                                                    │ [B] → MENU
```

### Tabela de transições por evento

| Estado | Evento | Condição extra | Próximo estado |
|---|---|---|---|
| **SPLASH** | BTN_A (peek) | — | MENU |
| **MENU** | BTN_A (peek) | item=0 "Iniciar" | GAMEPLAY |
| **MENU** | BTN_A (peek) | item=1 "Ranking" | nada (TODO) |
| **MENU** | BTN_A (peek) | item=2 "Sobre" | nada (TODO) |
| **MENU** | BTN_B (peek) | — | SPLASH |
| **GAMEPLAY** | BTN_START (fila) | qualquer sub | PAUSE |
| **GAMEPLAY** | gatilho de porta (tick) | — | troca de SALA |
| **GAMEPLAY** | BTN_Y (fila) | EXPLORANDO + at_equipment | sub→TERMINAL_ABERTO |
| **GAMEPLAY** | BTN_A (fila) | TERMINAL_ABERTO | sub→WAITING_CARD |
| **GAMEPLAY** | BTN_B (fila) | TERMINAL_ABERTO | sub→EXPLORANDO |
| **GAMEPLAY** | BTN_X (fila) | WAITING_CARD + carta correta | sub→ACTION_LOCK |
| **GAMEPLAY** | BTN_X/Y (fila) | WAITING_CARD + carta errada | fica em WAITING |
| **GAMEPLAY** | BTN_B (fila) | WAITING_CARD | sub→TERMINAL_ABERTO |
| **GAMEPLAY** | TICK | ACTION_LOCK ≥ 1500ms | sub→SYSTEM_DEPLOY |
| **GAMEPLAY** | TICK | SYSTEM_DEPLOY ≥ 4000ms | sub→EXPLORANDO |
| **GAMEPLAY** | TICK (engine) | vidas=0 | GAME_OVER (DERROTA) |
| **GAMEPLAY** | TICK (engine) | relógio ≥ 18:00 | GAME_OVER (VITÓRIA) |
| **PAUSE** | BTN_START (fila) | — | GAMEPLAY (retoma) |
| **PAUSE** | BTN_B (fila) | — | MENU |
| **GAME_OVER** | BTN_A (fila) | — | GAMEPLAY (retry) |
| **GAME_OVER** | BTN_B (fila) | — | MENU |
| **GAME_OVER** | TICK | phase ≥ 30000ms | SPLASH |

---

## 5. Sub-FSM do Gameplay

```
        ──[reset/nova run]──►  ┌─────────────┐
                               │ EXPLORANDO  │◄───────────── [B] ──────────────┐
                               └─────────────┘                                 │
                                     │                                         │
                      [Y + at_equipment=true]                                  │
                                     ▼                                         │
                          ┌──────────────────┐                                 │
                          │ TERMINAL_ABERTO  │◄─── [B] ────────────────┐      │
                          └──────────────────┘                         │      │
                                     │ [A]                             │      │
                                     ▼                                 │      │
                          ┌──────────────────┐                         │      │
                          │  WAITING_CARD    │──── [B] ───────────────►┘      │
                          └──────────────────┘                                │
                      [X correto] │      │ [X/Y errado — fica]                │
                                  ▼      │                                     │
                          ┌──────────────────┐                                │
                          │  ACTION_LOCK     │  1500ms automático             │
                          └──────────────────┘                                │
                                     │                                        │
                                     ▼                                        │
                          ┌──────────────────┐                                │
                          │  SYSTEM_DEPLOY   │  4000ms automático             │
                          └──────────────────┘                                │
                                     │                                        │
                                     └────────────────────────────────────────┘
```

**Importante:** quando a sub-FSM NFC está em TERMINAL_ABERTO, WAITING_CARD, ACTION_LOCK ou SYSTEM_DEPLOY, o tick da empresa **bloqueia movimento do player** (`return` imediato). Isso está correto.

**Importante:** as overlays de tarefa verde e amarela (A) não alteram a sub-FSM. São controladas inteiramente pelo screen_empresa via peek, e bloqueiam o tick inteiro enquanto abertas.

---

## 6. Cada Tela em Detalhe

### 6.1 SPLASH (`screen_splash.c`)

| Aspecto | Estado |
|---|---|
| Exibe logo/imagem inicial | ✅ (asset do SD card) |
| [A] → MENU | ✅ via `fsm_set_state(GAME_STATE_MENU)` (peek) |
| LEDs | ❌ nada (apagados) |

---

### 6.2 MENU (`screen_menu.c`)

| Aspecto | Estado |
|---|---|
| Navegação joystick cima/baixo | ✅ com debounce 300ms |
| Cursor visual ">" | ✅ |
| "Iniciar Expediente" → GAMEPLAY | ✅ |
| "Ranking" → tela de ranking | ❌ só `ESP_LOGI "TODO"` |
| "Sobre" → tela de créditos | ❌ só `ESP_LOGI "TODO"` |
| [B] → SPLASH | ✅ |
| LEDs arco-íris verde/amarelo/vermelho | ✅ animação suave 2.4s |

---

### 6.3 GAMEPLAY / RECEPÇÃO (`screen_recepcao.c`)

| Aspecto | Estado |
|---|---|
| Carrega layout do SD (`recepcao.json`) | ✅ |
| Spawn inicial (161,83) | ✅ |
| Spawn ao voltar do escritório (460,168) | ✅ |
| Movimento com joystick + colisão | ✅ |
| Y-sort entre entidades | ✅ |
| NPC troca frame quando player se aproxima | ✅ |
| Ícone piscante sobre NPC | ✅ (desaparece após diálogo) |
| Diálogo typewriter [A] avança / [B] pula | ✅ |
| Diálogo não repete na mesma sessão | ✅ (`s_dlg_played` na tela) |
| **⚠️ Diálogo repete após retry** | ❌ `s_dlg_played` é `static` — reiniciado só no boot, não no retry |
| Porta do escritório: **SEMPRE aberta** | ⚠️ Sem verificação se diálogo foi visto |
| [START] → PAUSE | ✅ via fila FSM |
| HUD (relógio + vidas) | ✅ mas relógio só avança após `gamestate_iniciar_expediente()` |
| LEDs | ❌ apagados na recepção (correto — expediente não iniciado) |

**Bug crítico de design:** O player pode entrar no escritório sem ver o diálogo da recepcionista. Se a intenção é que o diálogo sirva de "tutorial obrigatório", a porta precisa ser bloqueada até `s_dlg_played == true`.

---

### 6.4 GAMEPLAY / EMPRESA (`screen_empresa.c`)

| Aspecto | Estado |
|---|---|
| Carrega layout do SD (`empresa.json`) | ✅ |
| Spawn inicial (24,165) | ✅ |
| Movimento com joystick + colisão | ✅ |
| Y-sort entre entidades | ✅ |
| NPC_02 troca frame por proximidade | ✅ |
| NPC_03 não tem interação | ⚠️ Só visual — sem lógica |
| Ícone de tarefa (ícone verde + âmbar) | ✅ posicionado pelo crop offset |
| Prompt "[A]" aparece perto de tarefa | ✅ |
| Tarefa verde: overlay abre com [A] | ✅ |
| Tarefa amarela: overlay abre com [A] | ✅ |
| Bloqueio de movimento durante tarefa | ✅ (tick retorna cedo) |
| Bloqueio de movimento durante NFC | ✅ (guarda `!= EXPLORANDO`) |
| `gamestate_iniciar_expediente()` ao entrar | ✅ (idempotente) |
| [Y] → terminal NFC | ✅ (mock via X/Y) |
| Porta recepção → troca de sala | ✅ |
| [START] → PAUSE | ✅ via fila FSM |
| Callback tarefa concluída → registra no gamestate | ❌ só `ESP_LOGI` |
| Tarefa verde pode ser repetida | ❌ sem estado "já concluída" |
| LEDs vermelho durante ataque | ✅ progressivo 1-2-3 LEDs |

---

### 6.5 Tarefa Verde — overlay (`screen_tarefa_verde.c`)

| Aspecto | Estado |
|---|---|
| Carrega assets do SD | ✅ |
| Sorteia credenciais (2 inseguras + 2 seguras) | ✅ via `esp_random()` |
| Grid 2×2 navegável com joystick | ✅ |
| Step 1: selecionar usuário | ✅ |
| Step 2: selecionar senha | ✅ |
| Step 3: confirmar / corrigir | ✅ |
| Feedback erro (credenciais inseguras) | ✅ |
| [B] cancela e fecha | ✅ |
| Conclusão correta → chama callback | ✅ |
| Callback registra no gamestate | ❌ |
| Estado "tarefa concluída" (não repetir) | ❌ |
| `screen_tarefa_verde_reset()` | ❌ não existe (mas não tem estado persistente) |

---

### 6.6 Tarefa Amarela — overlay (`screen_tarefa_amarela.c`)

| Aspecto | Estado |
|---|---|
| Carrega assets do SD | ✅ |
| Estado HD persistente entre sessões de overlay | ✅ (`s_state_initialized`) |
| Navegação entre painel servidor / estoque | ✅ |
| Mecânica de pegar/soltar HD | ✅ |
| Verificação se servidor está completo (6 bons HDs) | ✅ |
| [B] cancela e fecha | ✅ |
| Conclusão → chama callback | ✅ |
| `screen_tarefa_amarela_reset()` chamado no retry | ✅ (engine.c) |
| Callback registra no gamestate | ❌ |
| Estado "tarefa concluída" (não repetir) | ❌ |

---

### 6.7 Tarefa Vermelha (DDoS)

| Aspecto | Estado |
|---|---|
| `screen_tarefa_vermelha.c` | ❌ **NÃO EXISTE** |
| Ataques vermelhos chegam via threat.c | ✅ (lógica de threat funcional) |
| Defesa via NFC (X/Y mock) | ✅ funcional como placeholder |
| Visual da defesa NFC | ❌ sem tela (terminal fica "invisível") |
| Feedback ao player que ataque está acontecendo | ⚠️ só LEDs vermelhos |

---

### 6.8 PAUSE (`screen_pause.c`)

| Aspecto | Estado |
|---|---|
| Tela de pausa existe | ✅ |
| [START] retoma | ✅ via fila FSM |
| [B] → MENU | ✅ via fila FSM |
| Relógio congelado durante pause | ✅ (`gamestate_tick` não é chamado quando pause) |
| Ameaças congeladas durante pause | ✅ (`gameplay_model_tick` verifica `GAME_STATE_GAMEPLAY`) |

---

### 6.9 GAME OVER (`screen_game_over.c`)

| Aspecto | Estado |
|---|---|
| Lê `gamestate_get_result()` para decidir VITÓRIA/DERROTA | ✅ |
| Carrega `ASSET_TELA_VITORIA` ou `ASSET_TELA_DERROTA` | ✅ |
| Título "PROMOVIDO!" (verde) ou "DEMITIDO" (vermelho) | ✅ |
| Hint de botões diferente para cada resultado | ✅ |
| [A] retry → GAMEPLAY (reseta tudo) | ✅ |
| [B] → MENU | ✅ |
| [30s idle] → SPLASH | ✅ |
| Exibe pontuação acumulada | ❌ (score não implementado) |

---

## 7. Subsistemas Paralelos

### 7.1 Gamestate (`gamestate.c`)

Armazena o estado persistente durante uma run:

| Campo | Funcional? | Detalhe |
|---|---|---|
| `s_elapsed_ms` → relógio 08:00–18:00 | ✅ | só avança quando `s_expediente_ativo=true` |
| `s_vidas` (3 iniciais) | ✅ | decrementado pelo engine quando ataque expira |
| `s_result` (EM_ANDAMENTO / VITÓRIA / DERROTA) | ✅ | lido pela tela de game over |
| `s_expediente_ativo` | ✅ | setado ao entrar no escritório pela 1ª vez |
| Score / pontuação | ❌ | `SCORE_VERDE/AMARELA/VERMELHO_BASE` definidos em `game_config.h` mas nunca usados |
| Tarefas concluídas (verde/amarela) | ❌ | não existe campo |

### 7.2 Sistema de Ameaças (`threat.c`)

| Aspecto | Funcional? |
|---|---|
| 3 tipos: DDoS, Ransomware, Propagação | ✅ |
| Spawn automático com intervalo configurável | ✅ |
| Timer de 20s por ataque (VERMELHO_TIMER_MS) | ✅ |
| `threat_progress_pct()` para LEDs | ✅ |
| Carta correta vs. errada vs. inútil | ✅ |
| Carta errada AGRAVA (reduz tempo em 50%) | ✅ |
| Máximo 1 ataque simultâneo | ✅ |
| **⚠️ Ataques começam mesmo sem expediente ativo** | ❌ |

### 7.3 LEDs WS2812 (`ws2812_hal.c` + lógica em `engine.c`)

Cada LED representa uma tarefa:

| LED | Tarefa associada | Cor normal |
|---|---|---|
| LED 0 | Tarefa Verde | Verde `(0, 80, 0)` |
| LED 1 | Tarefa Amarela | Amarelo `(100, 100, 0)` |
| LED 2 | Ataque Vermelho | Apagado (sem ataque) |

**Comportamento por estado de jogo:**

| Estado | Comportamento |
|---|---|
| MENU | Arco-íris verde/amarelo/vermelho, ciclo 2.4s |
| GAMEPLAY / RECEPCAO | Apagados |
| GAMEPLAY / EMPRESA (sem ataque) | LED0=verde, LED1=amarelo, LED2=apagado |
| GAMEPLAY / EMPRESA — ataque 0–29% | LED0=verde, LED1=amarelo, LED2=vermelho |
| GAMEPLAY / EMPRESA — ataque 30–59% | LED0=verde, LED1=vermelho, LED2=vermelho |
| GAMEPLAY / EMPRESA — ataque 60–84% | LED0=vermelho, LED1=vermelho, LED2=vermelho (sólido) |
| GAMEPLAY / EMPRESA — ataque 85–99% | Todos 3 piscam individualmente de forma aleatória/caótica em vermelho |
| Ataque expirou (vida perdida) | Animação: todos 3 piscam vermelho juntos 3× (1.08s), depois volta ao normal |
| Ataque mitigado (carta correta) | Animação: todos 3 piscam verde juntos 3× (1.08s), depois volta ao normal |
| PAUSE | Apagados |
| GAME_OVER | Apagados |

**Bônus de velocidade:** durante qualquer ataque ativo, a velocidade de movimento do player no escritório aumenta 50% — cria urgência para chegar ao terminal.

### 7.4 Entity System / Y-Sort

| Aspecto | Funcional? |
|---|---|
| Pool de entidades com capacidade fixa | ✅ |
| Carregamento de layout do SD (JSON) | ✅ |
| Y-sort dentro de `s_game_layer` | ✅ |
| `s_ui_layer` sempre na frente | ✅ (bug de camadas antigo — corrigido) |
| Debug overlay (X+Y 2s) | ✅ mostra collision boxes + pivot + sort_y |

### 7.5 HUD (`screen_hud.c`)

| Aspecto | Funcional? |
|---|---|
| Relógio in-game (08:00–18:00) | ✅ |
| Vidas (corações ou similar) | ✅ |
| Atualizado a cada tick | ✅ |

---

## 8. Tabela de Status

### Funcionalidades implementadas e funcionais ✅

- Boot completo (banner, init de todos os subsistemas)
- FSM macro: SPLASH → MENU → GAMEPLAY → PAUSE → GAME_OVER
- Sub-FSM NFC: EXPLORANDO → TERMINAL → WAITING → ACTION_LOCK → SYSTEM_DEPLOY
- Troca de salas RECEPCAO ↔ EMPRESA
- Retry e nova run (reseta gamestate + ameaças + tarefa amarela)
- Relógio 08:00–18:00 (só conta quando expediente ativo)
- Sistema de vidas (3 vidas, derrota por esgotamento)
- Vitória por chegar às 18:00
- Game over com resultado correto (PROMOVIDO / DEMITIDO)
- Diálogo da recepcionista (typewriter, multi-linha)
- Tarefa verde (selecionar usuário+senha seguros) — overlay completa
- Tarefa amarela (backup de HDs) — overlay completa
- Terminal NFC mock (X=correto, Y=errado)
- Sistema de ameaças (DDoS, Ransomware, Propagação) com timers
- LEDs de estado (arco-íris menu / tarefa/perigo escritório)
- Y-sort + debug overlay
- Pipeline de assets (SD card → `.bin`)

### Stubs (existe código mas incompleto) ⚠️

| Funcionalidade | O que existe | O que falta |
|---|---|---|
| Callbacks de tarefa concluída | `ESP_LOGI` | Registrar no gamestate, acumular score |
| "Ranking" no menu | Label + case vazio | Tela de ranking + dados de score |
| "Sobre" no menu | Label + case vazio | Tela de créditos |
| Terminal NFC | Mock X/Y funcional | NFC real (evento `FSM_EVT_NFC`) |
| Pontuação | Constantes definidas (`SCORE_VERDE`, etc.) | Acúmulo no gamestate + exibição |
| Tarefa vermelha (visual) | Lógica de ameaça funcional | `screen_tarefa_vermelha.c` |
| Diálogo recepcionista → desbloquear porta | Flag `s_dlg_played` existe | Verificar a flag antes de abrir `AREA_PORTA_EMPRESA` |

### Não implementado ❌

| Funcionalidade |
|---|
| `screen_tarefa_vermelha.c` — tela visual do ataque DDoS/Ransomware/Propagação |
| Score / pontuação no gamestate |
| Exibição de score no game over |
| Tela de RANKING_VIEW |
| Tela de CREDITOS |
| NFC real (substituir X/Y mock) |
| Estado "tarefa verde já concluída" (não repetir) |
| Estado "tarefa amarela já concluída" (não repetir) |
| Bloqueio da porta do escritório antes do diálogo |

---

## 9. Problemas e Inconsistências

### P1 — ⚠️ CRÍTICO DE DESIGN: Ataques começam antes do expediente

**Onde:** `engine.c:gameplay_model_tick()`
**Problema:** `threat_tick(dt_ms)` é chamado assim que `GAME_STATE_GAMEPLAY` está ativo, mas `gamestate_expediente_ativo()` só vira `true` quando o player entra no escritório pela 1ª vez. Se o player demora mais de 10s na recepção, um ataque vermelho surge — e expira — sem que ele possa fazer nada. Resultado: **perde uma vida sem poder se defender**.

```c
// ATUAL (bug):
static void gameplay_model_tick(uint32_t dt_ms)
{
    if (fsm_get_state() != GAME_STATE_GAMEPLAY) return;
    gamestate_tick(dt_ms);
    if (threat_tick(dt_ms)) {            // ← roda mesmo na recepção!
        gamestate_perder_vida();
    }
    ...
}

// CORRETO:
static void gameplay_model_tick(uint32_t dt_ms)
{
    if (fsm_get_state() != GAME_STATE_GAMEPLAY) return;
    gamestate_tick(dt_ms);
    if (!gamestate_expediente_ativo()) return;  // ← adicionar esta linha
    if (threat_tick(dt_ms)) {
        gamestate_perder_vida();
    }
    ...
}
```

---

### P2 — ⚠️ CRÍTICO DE DESIGN: Porta do escritório sempre aberta

**Onde:** `screen_recepcao.c:recepcao_tick()` linha 242
**Problema:** O player pode ir direto para o escritório sem falar com a recepcionista. O diálogo existe mas não tem consequência para a progressão.

```c
// ATUAL (sem bloqueio):
if (g && g->kind == AREA_PORTA_EMPRESA) {
    fsm_set_gameplay_sala(GAMEPLAY_SALA_EMPRESA);
    return;
}

// CORRETO (se diálogo for obrigatório):
if (g && g->kind == AREA_PORTA_EMPRESA) {
    if (!s_dlg_played) {
        /* Porta bloqueada: mostra prompt ou mensagem */
        return;
    }
    fsm_set_gameplay_sala(GAMEPLAY_SALA_EMPRESA);
    return;
}
```

**Decisão de design necessária:** o diálogo deve ser obrigatório?

---

### P3 — 🟠 MÉDIO: `s_dlg_played` não reseta no retry

**Onde:** `screen_recepcao.c`
**Problema:** `static bool s_dlg_played = false;` é uma variável estática de arquivo. Em C, variáveis `static` são inicializadas apenas uma vez (no boot). Após o jogador ver o diálogo na primeira run, `s_dlg_played = true` permanece assim em todas as runs subsequentes do mesmo boot. O NPC não falará mais mesmo em retry.

**Pergunta de design:** isso é intencional (para não repetir tutorial) ou bug?

**Se for bug** — adicionar `s_dlg_played = false;` no início de `screen_recepcao_build()`.

---

### P4 — 🟠 MÉDIO: Tarefa verde pode ser repetida infinitamente

**Onde:** `screen_empresa.c:empresa_tick()`
**Problema:** Não existe estado "tarefa verde concluída" no gamestate. Toda vez que o player se aproxima do PC e pressiona A, a tarefa abre com novas credenciais. O callback `on_tarefa_vd_done` só faz `ESP_LOGI`. Quando pontuação for implementada, isso permitirá farmar pontos infinitamente.

**Solução:** adicionar `bool s_tarefa_vd_feita` no gamestate; setar em `on_tarefa_vd_done`; checar antes de abrir.

---

### P5 — 🟠 MÉDIO: Tarefa amarela pode ser repetida após conclusão

**Mesmo problema do P4** mas para a tarefa amarela. `screen_tarefa_amarela_reset()` existe e é chamado no retry — mas não há estado "já concluída nesta run" que impeça reabertura.

---

### P6 — 🟡 MENOR: NPC_03 sem interação

**Onde:** `screen_empresa.c` — loop de busca de NPC no pool pega apenas o primeiro NPC.
NPC_03 (337,127) existe no layout mas nunca é referenciado. Sem efeito no gameplay, mas limita futuras interações.

---

### P7 — 🟡 MENOR: Score definido mas nunca acumulado

`SCORE_VERDE = 10`, `SCORE_AMARELA = 20`, `SCORE_VERMELHO_BASE = 50` estão em `game_config.h`. O gamestate não tem campo de score. A tela de game over não exibe pontuação.

---

### P8 — 🟡 MENOR: RANKING_VIEW e CREDITOS inalcançáveis

`GAME_STATE_RANKING_VIEW` e `GAME_STATE_CREDITOS` existem no enum mas:
- Nenhuma tela build/destroy existe para eles
- `sync_ui_to_macro()` cai no `default: /* sem tela ainda */`
- O menu diz "TODO" para essas opções

---

### P9 — 🟡 MENOR: Terminal NFC sem feedback visual

Quando o player abre o terminal NFC (Y → TERMINAL_ABERTO), não há nenhuma tela visível. O player vê o escritório normal enquanto a sub-FSM muda. Isso é ok para o mock atual, mas antes do NFC real precisará de uma tela de terminal.

---

## 10. O que falta para o jogo ser jogável

Ordenado por impacto:

### Bloco 1 — Correções críticas (jogo quebrado sem isso)

| # | O que | Arquivo | Complexidade |
|---|---|---|---|
| 1 | Ameaças só começam após expediente ativo | `engine.c` linha ~198 | 1 linha |
| 2 | Decisão: porta do escritório bloqueada até diálogo? | `screen_recepcao.c` | Baixa |
| 3 | Decisão: `s_dlg_played` reseta no retry? | `screen_recepcao.c` | 1 linha |

### Bloco 2 — Completar mecânicas de tarefa (core loop)

| # | O que | Arquivo | Complexidade |
|---|---|---|---|
| 4 | Gamestate: adicionar score + flags de tarefa concluída | `gamestate.c/.h` | Média |
| 5 | Callbacks de tarefa registram no gamestate | `screen_empresa.c` | Baixa |
| 6 | Tarefa verde: não reabre se já concluída na run | `screen_empresa.c` | Baixa |
| 7 | Tarefa amarela: não reabre se já concluída na run | `screen_empresa.c` | Baixa |

### Bloco 3 — Visual da defesa contra ataques (imersão)

| # | O que | Arquivo | Complexidade |
|---|---|---|---|
| 8 | `screen_tarefa_vermelha.c` — tela do ataque | novo arquivo | Alta |
| 9 | Terminal NFC: tela visual (mesmo que simples) | novo arquivo | Média |

### Bloco 4 — Menus e fim de jogo

| # | O que | Arquivo | Complexidade |
|---|---|---|---|
| 10 | Exibir score no game over | `screen_game_over.c` | Baixa |
| 11 | Tela de ranking (pelo menos top 3 da sessão) | novo arquivo | Média |
| 12 | Tela de créditos / "Sobre" | novo arquivo | Baixa |

---

## 11. Fluxo ideal pós-implementação do Bloco 1+2

```
[BOOT] → SPLASH → MENU → GAMEPLAY / RECEPCAO

  Recepção:
    └─ Porta BLOQUEADA até falar com NPC
    └─ NPC explica missão → s_dlg_played=true → porta ABRE
    └─ Entra no escritório

  Escritório:
    └─ Expediente começa → relógio + ameaças ativados
    │
    ├─ [A] perto do PC → tarefa verde
    │     Conclusão: score += 10, flag verde = feita, não reabre
    │
    ├─ [A] perto do rack → tarefa amarela
    │     Conclusão: score += 20, flag amarela = feita, não reabre
    │
    └─ [Y] perto de equip → terminal NFC
          Ataque DDoS/Ransomware/Propagação ativo:
            X (carta certa) → mitigado, score += 50 (+ bônus velocidade)
            Y (carta errada) → ataque agravado (-50% do tempo)
            B → aborta terminal
          Sem ataque: -1 retorna ao explorar

  18:00 → VITÓRIA → GAME_OVER (score final exibido)
  Vidas=0 → DERROTA → GAME_OVER

  [A] retry: reseta TUDO (relógio, vidas, score, flags de tarefa)
  [B] → MENU
```

---

*Documento gerado em 2026-06-08. Fonte: leitura integral de fsm.c, engine.c, gamestate.c, screen_recepcao.c, screen_empresa.c, screen_tarefa_verde.c, screen_tarefa_amarela.c, screen_game_over.c, screen_menu.c, game_config.h, fsm_states.h, fsm_gameplay.h.*
