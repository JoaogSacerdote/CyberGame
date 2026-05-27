# 05 — Regras FreeRTOS

> Tasks, filas, semáforos, mutex, ISR, notificações, task watchdog.
> Aplicação direta: `engine_task`, `button_reader_task`, `nfc_task`,
> `LVGL task`, `pmu_shutdown_monitor_task`.

## Mapa de tasks do CyberGame

| task | core | prio | stack | onde |
|---|---|---|---|---|
| `engine_task` | 0 | 6 | 4096 | `components/engine/engine.c` |
| `button_reader_task` | (qualquer) | (herdada) | (default) | `components/engine/engine.c` |
| `nfc_task` (interna) | (qualquer) | 4 | 4096 | `components/hardware/nfc_hal.c` |
| `joystick task` (interna) | 1 | 5 | 3072 | `components/hardware/joystick_hal.c` |
| LVGL task | 1 | (LVGL default) | (LVGL default) | `components/hal_bridge/hal_bridge.c` |
| `pmu_shutdown_monitor` | 0 | 3 | 3072 | `components/hardware/pmu.c` |

Atualizar esta tabela quando criar/remover task.

## Core affinity

Regra: **LVGL fica em Core 1 exclusivo**. GameEngine, hardware-bound
tasks ficam em Core 0. Audio (quando existir) acompanha Core 0.

Motivo: LVGL renderização é a única atividade que se beneficia de não
ser preempted por interrupts da rede/USB/etc., que tendem a cair em
Core 0.

Para nascer com afinidade:

```c
xTaskCreatePinnedToCore(my_task, "name", STACK, NULL, PRIO, &handle, CORE);
```

Nunca usar `xTaskCreate` (sem afinidade) para tasks de jogo.

## Prioridades

Escala usada:

| prio | uso |
|---|---|
| 1-2 | background / cleanup |
| 3-4 | monitors (pmu shutdown, nfc poll) |
| 5-6 | game / processing |
| 7-10 | hardware-critical (raro neste projeto) |
| 15+ | nunca |

Manter LVGL na prioridade default sugerida pelo IDF (`CONFIG_LV_OS_FREERTOS`
expõe via menuconfig). Não inflar.

## Stacks

Defaults razoáveis:

- Tarefa que só lê queue e processa: **3072 bytes**.
- Tarefa que usa `ESP_LOG`: **+1024** (formatação consome stack).
- Tarefa que toca LVGL: nada — LVGL roda em sua própria task.
- Tarefa com `printf`/`snprintf` longos: **+2048**.

Medir stack real com `uxTaskGetStackHighWaterMark` em dev. Cortar
folga > 1.5 KB; aumentar se sobrar < 256 bytes.

## Filas (`xQueue*`)

### Default

```c
static QueueHandle_t s_queue = NULL;
s_queue = xQueueCreate(DEPTH, sizeof(my_struct_t));
ESP_RETURN_ON_FALSE(s_queue, ESP_ERR_NO_MEM, TAG, "queue alloc failed");
```

Send/receive ver [03-event-model.md](03-event-model.md).

### Profundidade

- Eventos esparsos (botões, NFC): `16-32`.
- Streams contínuos (UART RX): calcular `(bytes_por_segundo / freq_consumer) * margem`.

### Política de fila cheia

**Produtor descarta + loga WARN**. Nunca bloquear produtor (vira deadlock
em cascata).

```c
if (xQueueSend(s_queue, &evt, 0) != pdTRUE) {
    ESP_LOGW(TAG, "queue cheia, descartando evento %d", evt.kind);
}
```

## Semáforos e mutex

### Binary semaphore para sinalizar (ISR → task)

```c
static SemaphoreHandle_t s_irq_sem = NULL;
s_irq_sem = xSemaphoreCreateBinary();

/* ISR */
xSemaphoreGiveFromISR(s_irq_sem, &hp_woken);

/* Task */
xSemaphoreTake(s_irq_sem, pdMS_TO_TICKS(timeout));
```

Use **task notification** em vez de semáforo se houver 1:1 producer-consumer
(mais barato — ver §notifications).

### Mutex para proteger dado compartilhado

```c
static SemaphoreHandle_t s_mutex = NULL;
s_mutex = xSemaphoreCreateMutex();

xSemaphoreTake(s_mutex, portMAX_DELAY);
/* acessar dado */
xSemaphoreGive(s_mutex);
```

**Não usar binary semaphore como mutex** — não tem priority inheritance.

### Mutex LVGL: regra de ouro

LVGL não é thread-safe. Toda escrita em árvore LVGL feita FORA da task
LVGL precisa de:

```c
lv_lock();
lv_obj_set_text(label, "novo");
lv_unlock();
```

**NUNCA chamar `ESP_LOGI` dentro de `lv_lock`** — log pode estourar UART
buffer e atrasar render, segurando o mutex por dezenas de ms.

(Regra reforçada em memória `feedback_lvgl_diff_gating`.)

## Task notifications (1:1)

Mais barato que semáforo. Default para "task acorda quando ISR/outra task
sinaliza":

```c
/* Receptor */
ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

/* Emissor (não-ISR) */
xTaskNotifyGive(target_task);

/* Emissor (ISR) */
vTaskNotifyGiveFromISR(target_task, &hp_woken);
```

Cabe um `uint32_t` de valor. Para enums pequenos é suficiente.

## ISR

### Regras estritas

1. **Marcar com `IRAM_ATTR`** se chamada de ISR direta (não trampoline):
   ```c
   static void IRAM_ATTR my_isr(void *arg) { ... }
   ```
2. **Curtíssima** — só sinalizar (`xQueueSendFromISR`, `xSemaphoreGiveFromISR`,
   `xTaskNotifyFromISR`).
3. **`*FromISR` variants** das funções FreeRTOS — nunca a versão normal.
4. **`BaseType_t hp_woken = pdFALSE`** + `portYIELD_FROM_ISR()` se foi setado.
5. **Sem `ESP_LOG*`** — não-bloqueante, não preempt-safe.
6. **Sem `malloc`, `printf`, qualquer alocação dinâmica**.
7. **Sem acesso à PSRAM** com cache desabilitada (algumas funções flash).

### Template

```c
static void IRAM_ATTR my_isr(void *arg)
{
    BaseType_t hp_woken = pdFALSE;
    /* sinalizar */
    xSemaphoreGiveFromISR(s_irq_sem, &hp_woken);
    if (hp_woken == pdTRUE) {
        portYIELD_FROM_ISR();
    }
}
```

### ISR de GPIO

Usar `hal_common::hal_isr_service_install_once()` no init do HAL. Registrar
handler com:

```c
gpio_isr_handler_add(BOARD_PIN_X, my_isr, (void *)(uintptr_t)context);
```

Passar contexto via `void *arg` (cast para `uintptr_t` evita warning).

## Software timers

`xTimerCreate` para debounce, timeouts, periódicos leves. Callback roda
em **timer service task** — tem stack próprio e prioridade configurável
via menuconfig.

Uso típico no projeto: debounce de botão.

```c
TimerHandle_t t = xTimerCreate("btn_debounce",
                               pdMS_TO_TICKS(50),
                               pdFALSE,                   // one-shot
                               (void *)(uintptr_t)idx,    // ID
                               debounce_cb);
xTimerResetFromISR(t, &hp_woken);   // chamado da ISR do botão
```

Regras:

- One-shot para timeout/debounce.
- Periódico só se realmente precisar (consome RAM).
- Callback **não** é ISR — pode chamar funções normais.

## Task watchdog

ESP-IDF tem **interrupt watchdog** (sempre ativo, default 300ms) e **task
watchdog** (opcional).

**Hoje**: não habilitamos task watchdog em `engine_task`. **Gap G8** em
`OPEN_QUESTIONS`.

Quando habilitar:

```c
#include "esp_task_wdt.h"

static void engine_task(void *pv)
{
    ESP_ERROR_CHECK(esp_task_wdt_add(NULL));   // self
    while (1) {
        /* trabalho */
        esp_task_wdt_reset();
    }
}
```

Config WDT em `menuconfig`:
- `CONFIG_ESP_TASK_WDT_EN=y`
- `CONFIG_ESP_TASK_WDT_TIMEOUT_S=5`

Tasks que **podem bloquear longamente** (queue receive com timeout
infinito): NÃO adicionar ao task watchdog, ou usar `esp_task_wdt_reset()`
no loop antes de bloquear.

## Logs em seções críticas

Resumo:

| onde | log permitido? |
|---|---|
| Task normal | ✅ qualquer nível |
| Callback de software timer | ✅ |
| ISR | ❌ **nunca** |
| Dentro de `lv_lock()`/`lv_unlock()` | ❌ |
| Dentro de mutex genérico | ⚠️ evitar; só `ESP_LOGW`/`ESP_LOGE` curtos |
| `_init` de HAL | ✅ 1× ao final ("HAL initialized at ...") |

## Anti-padrões

- ❌ `xTaskCreate` sem afinidade em código de jogo.
- ❌ Tarefa que faz `vTaskDelay(1)` em loop apertado (vira spin disfarçado).
- ❌ Mutex tomado em ordem inconsistente em códigos diferentes (deadlock).
- ❌ ISR que faz `xQueueSend` (não-FromISR).
- ❌ `xQueueReceive(..., portMAX_DELAY)` em task que está no task watchdog.
- ❌ Tomar `lv_lock()` dentro de outro lock (nesting).
- ❌ Inicializar task sem checar retorno (`xTaskCreatePinnedToCore` retorna
  `pdPASS`/`pdFAIL`).

## Como observar tasks em runtime

```c
// menuconfig: CONFIG_FREERTOS_USE_TRACE_FACILITY=y + CONFIG_FREERTOS_GENERATE_RUN_TIME_STATS=y
char buf[1024];
vTaskList(buf);
ESP_LOGI("DEBUG", "\n%s", buf);
vTaskGetRunTimeStats(buf);
ESP_LOGI("DEBUG", "\n%s", buf);
```

Útil em modo dev (`ui_debug`). **Caro em runtime** — não deixar em
produção.

## Referências externas

- *FreeRTOS Reference Manual* (https://www.freertos.org/Documentation/RTOS_book.html).
- INCB *ESP32 com IDF — O Guia Profissional* (Morais 2023) — caps
  FreeRTOS + Watchdogs.
- ESP-IDF API reference: `freertos`, `esp_task_wdt`, `driver/gpio` (ISR API).
