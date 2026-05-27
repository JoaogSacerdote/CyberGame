# 01 — Arquitetura (camadas e dependências)

> Quem pode incluir quem. Como decidir onde um arquivo novo deve morar.
> Como reconhecer violações.

## Camadas

Da mais baixa para a mais alta. **Dependência só desce.**

```
┌─────────────────────────────────────────┐
│ main/                                   │   bootstrap, ordem de init
├─────────────────────────────────────────┤
│ engine, ui, ui_debug, recovery          │   app + telas
├─────────────────────────────────────────┤
│ fsm, gamestate                          │   regras puras de gameplay
├─────────────────────────────────────────┤
│ assets, asset_store                     │   services (sprites, dialog)
├─────────────────────────────────────────┤
│ hal_bridge                              │   tradução HAL ↔ LVGL
├─────────────────────────────────────────┤
│ hardware (HAL)                          │   drivers ESP-IDF
├─────────────────────────────────────────┤
│ ESP-IDF + managed_components            │   plataforma
└─────────────────────────────────────────┘
```

## Matriz de dependências permitidas

Coluna = quem inclui; linha = quem é incluído.
✅ permitido, ❌ proibido (viola camada), ⚠️ permitido com cautela.

| ↓ inclui · → | main | engine | ui | ui_debug | recovery | fsm | gamestate | assets | asset_store | hal_bridge | hardware |
|---|---|---|---|---|---|---|---|---|---|---|---|
| main           | -  | ✅ | ⚠️ | ✅ | ✅ | ⚠️ | ⚠️ | ⚠️ | ✅ | ✅ | ✅ |
| engine         | ❌ | -  | ✅ | ❌ | ❌ | ✅ | ✅ | ❌ | ❌ | ❌ | ⚠️* |
| ui             | ❌ | ❌ | -  | ❌ | ❌ | ✅ | ✅ | ✅ | ❌ | ✅ | ⚠️* |
| ui_debug       | ❌ | ❌ | ❌ | -  | ❌ | ❌ | ❌ | ❌ | ❌ | ✅ | ⚠️* |
| recovery       | ❌ | ❌ | ❌ | ❌ | -  | ❌ | ❌ | ❌ | ✅ | ❌ | ✅ |
| fsm            | ❌ | ❌ | ❌ | ❌ | ❌ | -  | ✅ | ❌ | ❌ | ❌ | ⚠️* |
| gamestate      | ❌ | ❌ | ❌ | ❌ | ❌ | ❌ | -  | ❌ | ❌ | ❌ | ❌ |
| assets         | ❌ | ❌ | ❌ | ❌ | ❌ | ❌ | ❌ | -  | ✅ | ❌ | ❌ |
| asset_store    | ❌ | ❌ | ❌ | ❌ | ❌ | ❌ | ❌ | ❌ | -  | ❌ | ✅ |
| hal_bridge     | ❌ | ❌ | ❌ | ❌ | ❌ | ❌ | ❌ | ❌ | ❌ | -  | ✅ |
| hardware       | ❌ | ❌ | ❌ | ❌ | ❌ | ❌ | ❌ | ❌ | ❌ | ❌ | -  |

\* **⚠️ ATENÇÃO — gap conhecido**: hoje `engine`, `fsm`, `ui` e `ui_debug`
importam `button_hal.h` / `joystick_hal.h` diretamente. Isso é uma
**violação tolerada** até a refatoração G2 (criar `components/input/`
como service). Não introduzir NOVOS usos diretos de HAL nessas camadas.

## Regras invioláveis

### R1.1 — `components/hardware/*.h` não fala LVGL

Nenhum header de `components/hardware/` pode incluir `lvgl.h`, declarar
tipos LVGL, ou ter parâmetros do tipo `lv_*`. A integração com LVGL é
responsabilidade exclusiva de `components/hal_bridge/`.

**Motivo**: trocar de biblioteca gráfica ou rodar HALs em host de teste
(sem LVGL) tem que ser viável.

### R1.2 — `gamestate` é C puro

`components/gamestate/` não inclui FreeRTOS, ESP-IDF, ou nada que não
seja `<stdint.h>`, `<stdbool.h>`, `<stddef.h>`. É testável em host
(`test/host/test_gamestate.c`).

**Motivo**: lógica pura de regras de negócio é a coisa mais reutilizável
do projeto.

### R1.3 — Pinos só em `components/hardware/include/board_pins.h`

Ver [04-hal-contract.md](04-hal-contract.md) §pinout.

### R1.4 — Dependência só desce

Se você precisa que uma camada **inferior** chame algo de uma camada
**superior**, use **callback registration** (camada inferior expõe
`register_cb()`; camada superior registra função; inferior invoca via
ponteiro). Ver [04-hal-contract.md](04-hal-contract.md) §callbacks.

## Como decidir onde criar arquivo novo

Pergunte na ordem:

1. **Toca registrador, GPIO, periférico físico?** → `components/hardware/`
2. **Adapta HAL para LVGL?** → `components/hal_bridge/`
3. **Lê/escreve arquivos da NAND / abstração de storage?** → `components/asset_store/`
4. **Carrega sprite/diálogo/colisão de arquivo?** → `components/assets/`
5. **Regra pura de gameplay (puro C)?** → `components/gamestate/`
6. **Estado/transição de estados?** → `components/fsm/`
7. **Tela LVGL?** → `components/ui/` (ou `components/ui_debug/` para modo dev)
8. **Loop principal de jogo, scheduler de eventos?** → `components/engine/`
9. **USB CDC, OTA, modo manutenção?** → `components/recovery/`
10. **Bootstrap do firmware?** → `main/`

Se nenhuma resposta serve, é **provavelmente** um novo serviço — abra
discussão antes de criar componente novo.

## Cheatsheet de CMakeLists

Todo componente em `components/X/CMakeLists.txt` deve declarar
explicitamente seus REQUIRES. **Não confiar em transitividade** —
declare cada componente que você inclui diretamente.

Exemplo (`components/engine/CMakeLists.txt`):

```cmake
idf_component_register(
    SRCS "engine.c"
    INCLUDE_DIRS "include"
    REQUIRES
        "fsm"
        "hardware"      # ⚠️ violação tolerada (G2)
        "freertos"
        "log"
    PRIV_REQUIRES
        "ui"
)
```

- `REQUIRES`: componentes cujos headers públicos aparecem nos meus headers públicos.
- `PRIV_REQUIRES`: componentes cujos headers só aparecem nos meus `.c`.

## Como verificar violações

Comandos úteis (rodar na raiz):

```bash
# Quem inclui LVGL onde não deveria
grep -rn "lvgl.h" components/hardware/include/

# Quem inclui HAL direto fora de hardware/
grep -rn "button_hal.h\|joystick_hal.h\|nfc_hal.h" \
  components/{ui,fsm,engine}/

# Componentes sem REQUIRES explícito (heurística)
grep -L "REQUIRES" components/*/CMakeLists.txt
```

## Anti-padrões observados

- ❌ Componente declarando dependência transitiva (`REQUIRES "freertos"`
  só porque outro componente também declara).
- ❌ `#include "freertos/FreeRTOS.h"` em header público de
  `components/gamestate/`.
- ❌ Criar componente novo para 1 função (preferir adicionar a componente
  existente afim).
- ❌ Header público expondo `static` inline functions com lógica complexa
  (vira contrato implícito difícil de mudar).

## Referências externas

- Beningo, *Reusable Firmware Development*, cap 1 (Portable Firmware, 10
  qualidades) e cap 6 (HAL Design Process).
- Elecia White, *Making Embedded Systems*, cap 2 (System Architecture —
  Adapter Pattern, Layered View).
