# 03 — Modelo de eventos

> Como produzir e consumir eventos entre tasks. Por que NÃO usamos
> `esp_event`. Padrões de gating de periféricos.

## Decisão arquitetural: queue própria, não `esp_event`

O CyberGame usa **`QueueHandle_t` do FreeRTOS** com struct discriminada
(`fsm_event_t`) como mecanismo principal de eventos. Não usamos
`esp_event` (pub/sub do ESP-IDF).

### Por quê

| Critério | `esp_event` | `xQueueSend` |
|---|---|---|
| Tipo verificado em compile-time | ❌ (void* + handler genérico) | ✅ (struct discriminada) |
| Fan-out (N consumers) | ✅ | ❌ (1 queue = 1 consumer típico) |
| Ordem preservada | ✅ | ✅ |
| Overhead | task adicional + heap | mínimo |
| Adequado para 1 produtor → 1 consumer | overkill | ★ |
| Adequado para muitos eventos diferentes em pub/sub real | ★ | trabalhoso |

Hoje **temos 1 consumer principal** (`engine_task`) recebendo eventos
de N produtores (`button_reader_task`, NFC ISR, tick periódico). Padrão
clássico de queue resolve perfeitamente.

**Se aparecer caso real de pub/sub** (1 evento, N consumers que não
podem ser desserializados pelo engine), reabrir esta decisão.

## Estrutura do evento

```c
// components/fsm/include/fsm_events.h

typedef enum {
    FSM_EVT_BUTTON = 0,
    FSM_EVT_TICK,
    FSM_EVT_NFC_CARD,    // exemplo futuro
    /* ... */
} fsm_event_kind_t;

typedef struct {
    fsm_event_kind_t kind;
    union {
        struct {
            uint8_t id;       // button_id_t
            uint8_t state;    // button_state_t
        } button;
        struct {
            uint32_t dt_ms;
        } tick;
        struct {
            uint8_t uid[10];
            uint8_t uid_len;
        } nfc;
    } payload;
} fsm_event_t;
```

Regras:

- **`kind` é primeira field** — facilita inspeção em debugger.
- **`payload` é union, não void\***  — type-safe.
- **Tamanho total ≤ 32 bytes** — queue copy é por valor; eventos grandes
  matam latência.
- **Nada de pointers em payload** — ownership ambíguo, queue não pode
  liberar memória do remetente.

## Padrão produtor → consumer

### Produtor: task que lê hardware (não-ISR)

```c
// components/engine/engine.c

static void button_reader_task(void *pv)
{
    button_event_t bev;
    while (1) {
        // Bloqueia indefinidamente até HAL ter algo
        if (button_hal_get_event(&bev, UINT32_MAX)) {
            const fsm_event_t fev = {
                .kind = FSM_EVT_BUTTON,
                .payload.button = {
                    .id    = (uint8_t)bev.id,
                    .state = (uint8_t)bev.state,
                },
            };
            xQueueSend(s_event_queue, &fev, 0);
        }
    }
}
```

Regras:

- **Bloqueia em `_hal_get_event`** (1 sleep, 1 wake) em vez de polling.
- **`xQueueSend(..., 0)`** — sem timeout. Se a queue está cheia, **descarta**
  evento + loga `WARN`. Bloquear o produtor causa cascata.
- Conversão de tipo do HAL para `fsm_event_t` acontece **no produtor**,
  não no consumer.

### Produtor: ISR

```c
static void IRAM_ATTR my_isr(void *arg)
{
    BaseType_t hp_woken = pdFALSE;
    /* preparar evento ou só sinalizar */
    xQueueSendFromISR(s_queue, &evt, &hp_woken);
    if (hp_woken == pdTRUE) {
        portYIELD_FROM_ISR();
    }
}
```

Ver [05-freertos-rules.md](05-freertos-rules.md) §ISR para detalhes.

### Consumer: task que processa eventos

```c
// components/engine/engine.c

static void engine_task(void *pv)
{
    TickType_t last_tick = xTaskGetTickCount();
    fsm_event_t evt;

    while (1) {
        const TickType_t wait = pdMS_TO_TICKS(ENGINE_TICK_PERIOD_MS);
        if (xQueueReceive(s_event_queue, &evt, wait)) {
            fsm_handle_event(&evt);
        }
        // Tick periódico INDEPENDENTE de eventos
        const uint32_t dt = pdTICKS_TO_MS(xTaskGetTickCount() - last_tick);
        if (dt >= ENGINE_TICK_PERIOD_MS) {
            const fsm_event_t tick = {
                .kind = FSM_EVT_TICK,
                .payload.tick.dt_ms = dt,
            };
            fsm_handle_event(&tick);
            last_tick = xTaskGetTickCount();
        }
    }
}
```

Regras:

- **Timeout em `xQueueReceive`** = período do tick. Garante que sem
  eventos discretos, FSM ainda recebe tick para timers internos.
- **Tick separado do processamento de eventos** — não enfileirar tick
  em ISR/timer; calcular aqui no consumer.
- Cada evento → 1 chamada de `fsm_handle_event`.

## Gating de periféricos pela FSM

Padrão: **periférico fica desligado por default**; FSM liga ao entrar em
estado que precisa, desliga ao sair.

Exemplo (`components/hardware/nfc_hal.c`):

```c
esp_err_t nfc_hal_start_scanning(void);
esp_err_t nfc_hal_stop_scanning(void);
```

A FSM chama `start_scanning()` ao entrar em `GAMEPLAY_SUB_WAITING_CARD`
e `stop_scanning()` ao sair. Enquanto desligado, a task interna do NFC
fica bloqueada em semáforo (zero I2C, zero CPU).

Regras:

- **Default: periférico ENERGIZADO mas IDLE** (capaz de operar mas sem
  fazer I/O custoso).
- **Start/stop são idempotentes** — chamar duas vezes seguidas não
  quebra.
- **FSM é dona do estado on/off** — periférico não decide quando se
  ativar.

## Anti-padrões

- ❌ Pôr lógica de jogo no produtor (`button_reader_task` decidindo se o
  botão "vale" naquele estado).
- ❌ Múltiplos consumers da mesma queue ("vou criar outra task que
  também lê eventos do botão").
- ❌ Evento sem `kind` discriminante ("é só um int agora").
- ❌ Produtor bloqueando em `xQueueSend(..., portMAX_DELAY)` — vira
  deadlock quando consumer trava.
- ❌ Ler periférico no consumer (`engine_task` chamando `nfc_hal_wait_card`)
  — bloqueia a FSM. Sempre uma task dedicada por periférico bloqueante.

## Como inspecionar em runtime

- `ESP_LOG{I,W}` em todas as transições da FSM (já é regra em
  [02-fsm-pattern.md](02-fsm-pattern.md) §R2.1).
- `uxQueueMessagesWaiting(s_event_queue)` mostra eventos pendentes.
- Se queue encher: aumentar `ENGINE_QUEUE_DEPTH` (hoje 32) **OU** investigar
  por que o consumer está atrasado.

## Referências externas

- Beningo, *Reusable Firmware Development*, cap 3 (Fig 3-25 — UART RX
  design pattern: ISR → buffer → signal task → processing).
- FreeRTOS docs: Queues, Stream Buffers.
