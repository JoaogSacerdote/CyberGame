# 06 — Coding Standard

> Convenções alinhadas ao **ESP-IDF style guide oficial**
> (https://docs.espressif.com/projects/esp-idf/en/stable/esp32/contribute/style-guide.html).
> Este arquivo é nossa adaptação + extensões locais.

## Linguagem

- **C** para tudo. C99/C11 conforme ESP-IDF (não escolher).
- **C++ não** neste projeto (managed components podem ter, isolar).

## Naming

| coisa | convenção | exemplo |
|---|---|---|
| Função pública | `<componente>_<verbo>[_<obj>]` | `button_hal_get_event`, `fsm_handle_event` |
| Função estática | sem prefixo, snake_case | `handle_splash`, `state_name` |
| Variável local | snake_case | `int btn_idx` |
| Variável estática de arquivo | `s_<nome>` | `static QueueHandle_t s_queue` |
| Variável global | evitar; se inevitável: `g_<nome>` | (raro) |
| Constante / macro | `UPPER_SNAKE_CASE` | `BTN_DEBOUNCE_MS`, `ENGINE_TICK_PERIOD_MS` |
| Macro de pino | `BOARD_PIN_<COMP>_<SINAL>` | `BOARD_PIN_DISP_MOSI` |
| Tipo (struct/enum via typedef) | `<componente>_<obj>_t` | `button_event_t`, `joystick_data_t` |
| Valor de enum | `<COMP>_<VALUE>` ou `<COMP>_<OBJ>_<VALUE>` | `BTN_A`, `GAME_STATE_MENU` |
| Tag de log | `UPPER` curto, único por componente | `"BUTTON_HAL"`, `"FSM"`, `"ENGINE"` |
| Arquivo | snake_case, sem prefixo | `button_hal.c`, `fsm_states.h` |

### Prefixos por componente

| componente | prefixo |
|---|---|
| button_hal | `button_hal_` |
| joystick_hal | `joystick_hal_` |
| nfc_hal | `nfc_hal_` |
| display_hal | `display_hal_` |
| storage_hal | `storage_hal_` |
| pmu | `pmu_` |
| hal_common | `hal_` |
| hal_bridge | `hal_bridge_` |
| fsm | `fsm_` |
| gamestate | `gamestate_` |
| engine | `engine_` |
| asset_store | `asset_store_` |
| assets | `asset_loader_`, `dialog_loader_` |
| recovery | `recovery_` |
| ui | `ui_` (telas: `screen_<nome>`) |

## Indentação e whitespace

- **4 espaços**, sem tabs. (Verificável: `grep -nP '^\t' components/`).
- **Linha máxima 120 caracteres**. Exceder só se quebrar piorar
  legibilidade.
- 1 espaço após `if`, `for`, `while`, `switch`: `if (cond) {`.
- 1 espaço em volta de operadores binários: `a + b`, `x == 0`.
  Multiply/divide: opcional.
- Sem espaço em volta de `.`, `->`, unários.
- 1 linha em branco entre funções. Zero linhas no início/fim de função.
- **Sem trailing whitespace**.

## Braces

- **Função**: brace em linha separada.
  ```c
  esp_err_t button_hal_init(void)
  {
      /* ... */
  }
  ```
- **`if`/`for`/`while`/`switch`/`do`**: brace mesma linha.
  ```c
  if (cond) {
      do_a();
  } else if (other) {
      do_b();
  } else {
      do_c();
  }
  ```
- **`if` de 1 linha**: sempre usar braces. Não escrever `if (x) return;` em
  linha única (exceção: early return óbvio onde o bloco caberia em uma
  linha ainda mais curta — usar com bom senso).

## Headers

### Estrutura obrigatória

```c
#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/* declarações */

#ifdef __cplusplus
}
#endif
```

- `#pragma once` (não macro guard `#ifndef X_H_`).
- `extern "C"` sempre, mesmo se não há plano de C++.
- Includes em ordem (§includes abaixo).
- Sem `static` em header (exceto inline triviais).

### `.c` correspondente

Primeira linha: `#include` do próprio header.

```c
#include "button_hal.h"          // próprio header primeiro
#include "board_pins.h"          // outros do mesmo componente
#include "hal_common.h"

#include "freertos/FreeRTOS.h"   // ESP-IDF + FreeRTOS
#include "freertos/queue.h"
#include "freertos/timers.h"
#include "driver/gpio.h"
#include "esp_check.h"
#include "esp_log.h"
```

## Include order

De cima pra baixo:

1. **Próprio header** (se for `.c`).
2. **Outros headers do mesmo componente**.
3. **C standard** (`<stdint.h>`, `<stdbool.h>`, `<string.h>`).
4. **POSIX** (`<sys/queue.h>`).
5. **FreeRTOS** (`"freertos/FreeRTOS.h"` antes de outros `freertos/*`).
6. **ESP-IDF drivers** (`"driver/gpio.h"`, etc.).
7. **ESP-IDF utilitários** (`"esp_check.h"`, `"esp_log.h"`).
8. **Outros componentes do projeto** (`"fsm.h"`, etc.).
9. **Privados** (`<componente>_internal.h`).

Cada bloco separado por linha em branco. Dentro do bloco, ordem
alfabética **quando isso não impacta semântica** (FreeRTOS exige
`FreeRTOS.h` primeiro).

## Asserts e tratamento de erros

### Para `esp_err_t`

```c
ESP_RETURN_ON_ERROR(some_call(), TAG, "context %d", arg);
ESP_RETURN_ON_FALSE(ptr != NULL, ESP_ERR_NO_MEM, TAG, "alloc failed");
ESP_GOTO_ON_ERROR(other_call(), err, TAG, "context");
```

Nunca usar `assert(rc == ESP_OK)` para `esp_err_t` — use as macros.

### `ESP_ERROR_CHECK`: só no `main/`

`ESP_ERROR_CHECK(x)` chama `abort()` se falhar. Apropriado para
fail-fast no boot. Em HAL/components, **propagar** com
`ESP_RETURN_ON_ERROR` ou similar.

Exceção: comando **antigo** que já era `ESP_ERROR_CHECK` em código
funcional — não introduzir mudança sem motivo. Mas **novo** comando em
`_init` nunca usa `ESP_ERROR_CHECK` (regra do usuário, memória
`feedback_no_unauthorized_fixes`).

### Plain `assert()`

Para violações de invariante interno (bug do programador, não erro de
runtime). Ex.:

```c
assert(s_initialized);   // chamou função sem init
assert(idx < ARRAY_LEN); // out of bounds programático
```

Função dentro de `assert()` **não pode ter side effects** — ela é
removida quando `NDEBUG` é definido.

### Sem assertions para erro de runtime

Recuperável → retornar `esp_err_t`. Não usar `assert(file_exists)`.

## Comentários

- `//` para single line, `/* */` para multi-line.
- **PT-BR** (sem acentos para evitar problemas de encoding — projeto já
  segue isso).
- Comentar **por quê**, não **o quê**.
- Não comentar para desabilitar código sem explicação. Se desabilitar,
  comentar o motivo:
  ```c
  // TODO: reabilitar quando feedback_hal existir (ver OPEN_QUESTIONS B2)
  // feedback_hal_play(SFX_SUCCESS);
  ```
- Sem `#if 0 ... #endif` blocos sem justificativa.
- Cabeçalho de arquivo desnecessário (sem nome de autor, data, copyright
  por arquivo — git tem isso).

### Doxygen — não obrigatório, mas bem-vindo

Headers públicos de HAL **podem** usar tags Doxygen:

```c
/**
 * Inicializa o button_hal.
 *
 * @return ESP_OK se ok; ESP_ERR_NO_MEM em falha de alocação.
 *
 * Idempotente — múltiplas chamadas são seguras.
 */
esp_err_t button_hal_init(void);
```

Não é validado por CI hoje. Se for adicionar, ser consistente no header
inteiro.

## Static vs public

> "Qualquer função ou variável usada em só um arquivo deve ser `static`"
> — ESP-IDF style guide.

Aplicar **sempre**. Símbolos exportados acidentalmente viram dependência
implícita.

## Memória

- Stack > heap quando possível.
- Heap: `heap_caps_malloc` em vez de `malloc` quando precisar de capability
  (`MALLOC_CAP_DMA`, `MALLOC_CAP_SPIRAM`).
- `free()` sempre que `malloc()`. Sem leaks tolerados.
- Buffers grandes (>1 KB): em `static` ou `heap_caps_malloc`, nunca em
  stack.

## Atomicidade e thread safety

- `volatile` em variáveis lidas em ISR + task (mas `volatile` não dá
  atomicidade composta).
- Para flag simples (1 word): leitura/escrita é atômica em ESP32-S3.
- Para state composto: mutex (ver [05-freertos-rules.md](05-freertos-rules.md)).
- `__atomic_*` builtins do GCC quando precisar de RMW (read-modify-write)
  atômico — ver `storage_hal.c` para exemplo (`s_xfer_busy`).

## Logging

```c
static const char *TAG = "MEU_MODULO";

ESP_LOGI(TAG, "evento %s ocorreu", name);
ESP_LOGW(TAG, "condicao suspeita %d", val);
ESP_LOGE(TAG, "falha critica: %s", esp_err_to_name(err));
```

- `TAG` é `static const char *`, declarado no início do `.c`.
- Níveis: `D` (debug, default off), `I` (info), `W` (warn), `E` (error).
- `ESP_LOGD` para chatter de development — não deixar `LOGI` em loop apertado.

## CMakeLists

```cmake
idf_component_register(
    SRCS "main.c" "helper.c"
    INCLUDE_DIRS "include"
    REQUIRES
        "freertos"
        "log"
        "esp_check"
    PRIV_REQUIRES
        "esp_timer"
)
```

- Lowercase para comandos/funções/variáveis locais.
- 4 espaços de indent.
- 1 SRC por linha quando >2 arquivos.
- REQUIRES em ordem alfabética **quando não importa semântica**.
- Separar REQUIRES (públicos) de PRIV_REQUIRES (privados).

## Encoding e EOL

- **UTF-8 sem BOM**.
- **LF**, não CRLF. (`git config core.autocrlf input` no Windows.)

## Formatter

Não há formatter automatizado neste projeto (ainda). ESP-IDF oficial
usa `astyle` com `tools/ci/astyle-rules.yml`. Considerar adicionar
quando o projeto crescer.

## Verificações úteis (rodar antes de PR)

```bash
# tabs proibidos
grep -nP '^\t' components/ main/ -r

# linhas > 120 chars
awk 'length > 120 {print FILENAME":"NR}' components/**/*.{c,h} main/*.c

# include de LVGL em hardware/
grep -rn "lvgl.h" components/hardware/

# HAL direto em fsm/ui/engine
grep -rn "button_hal.h\|joystick_hal.h\|nfc_hal.h" \
  components/{ui,fsm,engine}/

# símbolos não-static suspeitos
nm build/*.elf | grep ' T ' | grep -v ' main\| app_main\| esp_'
```

## Referências externas

- ESP-IDF style guide oficial (URL no topo).
- Beningo, *Reusable Firmware Development*, cap 1 (Portability Issues
  in C — data types, structures, preprocessor).
- Existing code as canonical example — quando em dúvida, ler 2-3
  arquivos vizinhos e seguir o padrão observado.
