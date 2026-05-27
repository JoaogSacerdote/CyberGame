# 02 — Padrão de FSM

> Como modelar máquinas de estado neste projeto. Aplicação direta em
> `components/fsm/`.

## Modelo adotado: state-centric com sub-estado hierárquico

Para cada FSM:

1. **Enum** com todos os estados possíveis, declarado em `_states.h`.
2. **Função única `_handle_event(const event_t *evt)`** com `switch(state)`
   externo e tratamento de eventos por estado.
3. **Funções `set_state()` privadas** que logam transições.
4. **Getters `get_state()` thread-safe** (leitura atômica de uma variável estática).

Referência: Elecia White cap 5, opção "state-centric state machine".
Escolhida sobre table-driven porque temos poucos estados (≤20 totais) e
poucas transições — table-driven só compensa em FSMs grandes.

## Hierarquia macro / sub / sala

A FSM principal do CyberGame tem **3 dimensões**:

```c
// components/fsm/include/fsm_states.h

typedef enum {
    GAME_STATE_SPLASH,
    GAME_STATE_MENU,
    GAME_STATE_GAMEPLAY,       // ← entra na hierarquia
    GAME_STATE_PAUSE,
    GAME_STATE_GAME_OVER,
    GAME_STATE_RANKING_VIEW,
    GAME_STATE_CREDITOS,
} game_state_t;

typedef enum {
    GAMEPLAY_SUB_EXPLORANDO,
    GAMEPLAY_SUB_TERMINAL_ABERTO,
    GAMEPLAY_SUB_WAITING_CARD,
    GAMEPLAY_SUB_ACTION_LOCK,
    GAMEPLAY_SUB_SYSTEM_DEPLOY,
} gameplay_substate_t;

typedef enum {
    GAMEPLAY_SALA_RECEPCAO,
    GAMEPLAY_SALA_EMPRESA,
    GAMEPLAY_SALA_MAX,
} gameplay_sala_t;
```

Regras:

- **macro** muda em transições grandes (menu → gameplay → pause).
- **sub** só significa algo dentro de `GAME_STATE_GAMEPLAY`. Em outros
  macro-estados, sub deve ser ignorado.
- **sala** só significa algo dentro de `GAMEPLAY_SUB_EXPLORANDO`. Em
  sub-estados de terminal/ataque, sala é cenário de fundo.

## Anatomia de uma FSM bem-comportada

```c
// components/fsm/fsm.c

static const char *TAG = "FSM";
static game_state_t s_current = GAME_STATE_SPLASH;

void fsm_handle_event(const fsm_event_t *evt)
{
    switch (s_current) {
        case GAME_STATE_SPLASH:
            handle_splash(evt);
            break;
        case GAME_STATE_MENU:
            handle_menu(evt);
            break;
        /* ... */
        default:
            ESP_LOGE(TAG, "estado invalido: %d", s_current);
            break;
    }
}

static void set_state(game_state_t next)
{
    if (next == s_current) return;
    ESP_LOGI(TAG, "macro %s -> %s",
             state_name(s_current), state_name(next));
    s_current = next;
}
```

## Regras

### R2.1 — Toda transição é logada

Use `ESP_LOGI` com formato `"<dimensao> <de> -> <para>"`:

```
[I] FSM: macro MENU -> GAMEPLAY
[I] FSM: [GAMEPLAY] sub EXPLORANDO -> TERMINAL_ABERTO
[I] FSM: [GAMEPLAY] sala RECEPCAO -> EMPRESA
```

Não pular log de transição "porque é frequente" — quando algo der errado,
esse log é o que vai te dizer onde o jogo ficou preso.

### R2.2 — Função `state_name(state)` em cada FSM

Para cada enum de estado, gerar função `static const char *<dim>_name(<dim>_t)`
que retorna string. Usado em logs e debug. Adicionar `default: return "INVALID"`.

### R2.3 — Set state é função, não atribuição direta

Nunca fazer `s_current = X` em código fora da função `set_state()` (ou
equivalente). Garante: log centralizado + chance de validar transição
proibida + hook futuro para callbacks.

### R2.4 — `_set_state(next)` no-op se já está no estado

```c
if (next == s_current) return;
```

Sem isso, transições idempotentes spam log.

### R2.5 — Substate reseta ao entrar/sair de macro

Quando `GAMEPLAY` sai, `s_sub` é resetado para um valor neutro (ex.:
`GAMEPLAY_SUB_EXPLORANDO`). Quando entra, idem. Evita herança de
sub-estado entre sessões.

### R2.6 — Sala invalida flags relacionadas

Trocar de sala invalida automaticamente flags que dependiam da sala
anterior (ex.: `player_at_equipment`). Documentado em código:

```c
// components/fsm/fsm.c:fsm_set_gameplay_sala
s_sala_prev = s_sala;
s_sala = sala;
s_player_at_equipment = false;  // invalida — nova sala precisa reafirmar
```

### R2.7 — Eventos têm tipo discriminado

Ver [03-event-model.md](03-event-model.md). Estrutura:

```c
typedef struct {
    fsm_event_kind_t kind;       // discriminante
    union {
        struct { uint8_t id; uint8_t state; } button;
        struct { uint32_t dt_ms; } tick;
        /* ... */
    } payload;
} fsm_event_t;
```

Não usar `void *payload` cru — perde checagem de tipo.

### R2.8 — Tick separado de eventos discretos

FSM deve receber `FSM_EVT_TICK` periodicamente (a cada `ENGINE_TICK_PERIOD_MS`)
para timers/timeouts. Eventos discretos (botões, NFC) chegam por
demanda. **Tick não é botão**.

### R2.9 — Getters thread-safe sem mutex

Para state simples (1 word), leitura atômica é safe em ESP32-S3
(arquitetura permite `int` atômico). Usar:

```c
game_state_t fsm_get_state(void) { return s_current; }
```

Sem mutex. Se precisar de leitura coerente de múltiplas variáveis,
**aí** usar mutex (e documentar).

### R2.10 — Side effects em transição: na entrada do novo estado

Ex.: ligar NFC scanner ao entrar em `WAITING_CARD`, desligar ao sair.
Padrão: `on_enter(state)` chamado dentro de `set_state()` antes da
atualização da variável.

## Anti-padrões

- ❌ Switch sobre state dentro de switch sobre evento (use o externo
  sobre **state** — mais legível e segue padrão Beningo).
- ❌ Estado "transient" com ID próprio só pra fazer side effect (use
  `on_enter`).
- ❌ Sub-estados com nomes que duplicam macro (`MENU_PRINCIPAL_IDLE` em
  vez de só `MENU`).
- ❌ Estado "ERROR" genérico — prefira retornar ao estado anterior
  conhecido + logar.

## Quando NÃO usar FSM

- Lógica linear sem ramificação (`fade_in → fade_out → exit`) — usa
  função sequencial com `vTaskDelay`.
- Combinação 2D de muitas flags independentes — vira matriz, não FSM.
- Eventos puramente reativos sem estado — `if`/`switch` direto.

## Referências externas

- Elecia White, *Making Embedded Systems*, cap 5 (State Machines —
  comparação das 5 implementações).
- Beningo, *Reusable Firmware Development*, cap 3 (Driver Models — UART
  RX design pattern com FSM implícita).
