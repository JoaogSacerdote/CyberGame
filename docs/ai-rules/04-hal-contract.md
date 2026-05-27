# 04 — Contrato HAL

> Regras dos HALs em `components/hardware/`. Aplicação direta:
> `button_hal`, `joystick_hal`, `nfc_hal`, `display_hal`, `storage_hal`,
> `pmu`, `hal_common`.

## O que é um HAL neste projeto

Camada que **fala ESP-IDF** (`driver/gpio.h`, `esp_lcd_*`, `esp_adc/*`,
`driver/spi_master.h`, `driver/i2c_master.h`, etc.) e **expõe interface
agnóstica ao game**.

HALs **não conhecem**:
- LVGL (vai em `hal_bridge`)
- FSM, gamestate, regras de jogo
- Outros HALs (exceto via `hal_common` para infra compartilhada)

## Anatomia mínima de um HAL

### Header público (`include/<nome>_hal.h`)

```c
#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Tipos públicos (enums e structs). Prefixo do componente. */
typedef enum {
    BTN_A = 0,
    BTN_B,
    /* ... */
    BTN_MAX_COUNT,
} button_id_t;

/* API. Prefixo + verbo. */
esp_err_t      button_hal_init(void);
bool           button_hal_get_event(button_event_t *event, uint32_t timeout_ms);
button_state_t button_hal_peek(button_id_t id);

#ifdef __cplusplus
}
#endif
```

### Source (`<nome>_hal.c`)

```c
#include "<nome>_hal.h"
#include "board_pins.h"        // ← pinos
#include "hal_common.h"        // ← se usa ISR service

/* IDF + FreeRTOS na ordem do style guide (06-coding-standard.md §includes) */
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "driver/gpio.h"
#include "esp_check.h"
#include "esp_log.h"

static const char *TAG = "<NOME>_HAL";

/* Constantes locais (não-pinos). Pinos são SEMPRE BOARD_PIN_* de board_pins.h. */
#define <NOME>_QUEUE_DEPTH    16

/* State estática prefixada com s_ (style guide ESP-IDF). */
static QueueHandle_t  s_queue = NULL;

/* Funções internas: static, sem prefixo público. */
static void IRAM_ATTR my_isr(void *arg) { /* ... */ }

/* API pública: prefixo + esp_err_t. */
esp_err_t button_hal_init(void) { /* ... */ }
```

## Regras

### R4.1 — Pinos só de `board_pins.h`

Nunca declarar `#define MY_PIN GPIO_NUM_X` num `_hal.c`. Adicionar pino
novo em `components/hardware/include/board_pins.h` com prefixo
`BOARD_PIN_<COMPONENTE>_<SINAL>` e referência do bus/contexto físico.

Exceção: **canais ADC** (não são GPIOs no contrato do driver). Manter
local com `_Static_assert` ligando ao `BOARD_PIN_*_ADC` correspondente
(ver `joystick_hal.c`).

### R4.2 — Init retorna `esp_err_t`

Toda função de init de HAL retorna `esp_err_t`. Padrão:

```c
esp_err_t xxx_hal_init(void)
{
    if (s_initialized) return ESP_OK;   // idempotência
    /* ... */
    ESP_RETURN_ON_ERROR(some_call(), TAG, "context");
    /* ... */
    s_initialized = true;
    return ESP_OK;
}
```

Idempotência **obrigatória** — múltiplos chamadores de `init()` é cenário
normal (boot + recovery + dev mode).

### R4.3 — `_init` não encadeia `ESP_ERROR_CHECK` em comando novo

Regra do usuário (memória `feedback_no_unauthorized_fixes`): se uma
inicialização **NOVA** falhar, propagar via `ESP_RETURN_ON_ERROR`, não
abortar com `ESP_ERROR_CHECK`. Aborto deve ser decisão do `main/`, não
do HAL.

### R4.4 — Tipos públicos têm sufixo `_t`

`button_event_t`, `joystick_data_t`, `nfc_card_t`. Enum também:
`button_id_t`, `button_state_t`. Estilo ESP-IDF.

### R4.5 — Sem LVGL, sem types LVGL

`#include "lvgl.h"` é proibido em headers de `components/hardware/`.
Verificável com:

```bash
grep -rn "lvgl.h\|lv_disp\|lv_obj\|lv_color" components/hardware/include/
```

### R4.6 — Callbacks: registration explícito

Quando HAL precisa avisar uma camada superior de evento assíncrono
(ex.: DMA terminou), expor:

```c
typedef void (*xxx_hal_cb_t)(void *user_ctx);

esp_err_t xxx_hal_register_cb(xxx_hal_cb_t cb, void *user_ctx);
```

Não usar `extern` de função, callback global, ou flag polled.

Hoje implementado em `display_hal_register_trans_done_cb`. **Gap**: HALs
de input (button, joystick) ainda não têm. Quando implementar, seguir
esse padrão.

### R4.7 — Bloqueante vs não-bloqueante: explicitar

API que pode bloquear inclui parâmetro `uint32_t timeout_ms`:

```c
bool button_hal_get_event(button_event_t *event, uint32_t timeout_ms);
```

Convenção:
- `0` = polling (retorna imediatamente).
- `UINT32_MAX` = bloqueia indefinidamente (`portMAX_DELAY`).
- valor > 0 = bloqueia até N ms.

API que **nunca bloqueia** não recebe `timeout_ms`:

```c
button_state_t button_hal_peek(button_id_t id);  // snapshot atômico
```

Documentar em header se peek é thread-safe (geralmente sim para 1 word).

### R4.8 — `_init` sem parâmetros vs com config struct

**Hoje**: a maioria dos HALs do projeto usa `_init(void)`. Pinos vêm de
`board_pins.h`, constantes de defines locais.

**Recomendação Beningo tip #9**: passar ponteiro para config struct.

**Convenção para HALs novos**: aceitar ponteiro para config opcional.
`NULL` = usar defaults.

```c
typedef struct {
    uint16_t debounce_ms;
    uint8_t  queue_depth;
} button_hal_config_t;

esp_err_t button_hal_init(const button_hal_config_t *cfg);  // cfg=NULL ok
```

Migrar HALs existentes para isso é tarefa G4 em `OPEN_QUESTIONS`.

### R4.9 — ISR usa `IRAM_ATTR` e é minúscula

```c
static void IRAM_ATTR my_isr(void *arg)
{
    BaseType_t hp_woken = pdFALSE;
    xQueueSendFromISR(s_queue, &evt, &hp_woken);
    if (hp_woken == pdTRUE) portYIELD_FROM_ISR();
}
```

Ver [05-freertos-rules.md](05-freertos-rules.md) §ISR.

### R4.10 — Periféricos compartilhando bus: dono explícito

Display e NAND compartilham SPI2_HOST (MOSI=15, SCK=16). Regra:

- **Display é dono do bus** (`spi_bus_initialize`) porque exige
  `max_transfer_sz` muito maior.
- **NAND tolera "already initialized"** e só adiciona device
  (`spi_bus_add_device`).

Em código:

```c
// storage_hal.c
const esp_err_t ret = spi_bus_initialize(STORAGE_SPI_HOST, &bus_cfg, ...);
if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) return ret;
```

Para periférico novo compartilhando bus existente: documentar em
comentário quem é o dono.

### R4.11 — ISR service do GPIO é instalado UMA vez

Múltiplos HALs precisam de ISR em GPIO. Usar
`hal_common::hal_isr_service_install_once()` em vez de
`gpio_install_isr_service` direto. Evita log "already installed" + race
condition.

### R4.12 — `_init` não chama tasks vTaskDelay > 100ms sem motivo documentado

`vTaskDelay(pdMS_TO_TICKS(50))` para estabilizar VCC do display: OK.
`vTaskDelay(pdMS_TO_TICKS(500))` "por garantia": ❌.

Cada delay > 100ms tem comentário com motivo físico.

## Estrutura de pastas de um HAL

```
components/hardware/
├── include/
│   ├── board_pins.h          ← pinos de TODOS os HALs
│   ├── hal_common.h          ← helpers compartilhados (ISR service)
│   ├── button_hal.h          ← header público
│   └── ...
├── button_hal.c              ← implementação
├── hal_common.c
├── ...
├── CMakeLists.txt
└── idf_component.yml         ← deps managed components
```

`button_hal/` não é subpasta — cada HAL é um arquivo na raiz de
`hardware/`. Subpasta só justifica se tiver ≥3 arquivos.

## Antes de adicionar HAL novo

Checklist:

- [ ] Pinos adicionados em `board_pins.h` com `BOARD_PIN_<X>_*`?
- [ ] Header tem `#pragma once`, `extern "C"`, tipos com `_t`?
- [ ] `init` retorna `esp_err_t` e é idempotente?
- [ ] Função pública prefixada com `<nome>_hal_`?
- [ ] State estática com `s_`?
- [ ] Sem `lvgl.h`?
- [ ] `CMakeLists.txt` lista REQUIRES explicitamente?
- [ ] Se precisa de bus compartilhado: tolera "already initialized"?
- [ ] Se precisa de ISR GPIO: usa `hal_isr_service_install_once()`?
- [ ] Se precisa de callback: API `register_cb()` existe?
- [ ] `_Static_assert` para invariantes (ex.: canal ADC vs pino esperado)?

## Referências externas

- Beningo, *Reusable Firmware Development*, cap 2 (10 características de
  HAL) + cap 6 (Design Process, 7 passos + 10 tips).
- Elecia White, *Making Embedded Systems*, cap 2 (Adapter Pattern) +
  cap 4 (Board-Specific Header File + Facade Pattern).
- ESP-IDF style guide oficial.
- `CyberGameCore/_AGENT/MOC_hardware.md` (vault privado) — mapa
  detalhado de cada HAL existente.
