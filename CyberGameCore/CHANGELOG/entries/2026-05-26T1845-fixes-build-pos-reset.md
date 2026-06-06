---
date: 2026-05-26 18:45
type: edit
files:
  - components/fsm/CMakeLists.txt
  - components/fsm/include/fsm_states.h
  - components/fsm/include/fsm.h
  - components/ui/include/ui_internal.h
session: bring-up placa nova
---

# Fixes de build pós-reset — HEAD estava com código não-compilável

## Motivo
Após `git reset --hard HEAD` (entrada T1815), o build falhou em vários componentes. Diagnóstico: o commit `0490971` ("refactor(game-logic): elevar UI/assets ao nível pré-profissional Fases 1-4") foi commitado em estado INCONSISTENTE — referenciava funções/símbolos que ainda não tinham declarações em headers. Sua versão não commitada já tinha esses fixes mas foi descartada no reset.

## Erros encontrados

1. `fsm.c:6: fatal error: gamestate.h: No such file or directory`
2. `fsm.c: 'GAME_STATE_GAME_OVER' undeclared` (4 ocorrências)
3. `screen_hud.c: 'UI_HUD_HEIGHT_PX' undeclared`
4. `screen_empresa.c/screen_recepcao.c: implicit declaration of screen_hud_build/destroy/tick`
5. `screen_empresa.c: implicit declaration of fsm_set_player_at_equipment`

## Antes / Depois

### `components/fsm/CMakeLists.txt`
**Antes:**
```cmake
REQUIRES "log" "hardware"
```
**Depois:**
```cmake
REQUIRES "log" "hardware" "gamestate"
```

### `components/fsm/include/fsm_states.h`
**Antes:** enum sem `GAME_STATE_GAME_OVER`
**Depois:** adicionado após `GAME_STATE_PAUSE`:
```c
GAME_STATE_GAME_OVER,
```

### `components/fsm/include/fsm.h`
**Antes:**
```c
#include "esp_err.h"
#include "fsm_states.h"
#include "fsm_events.h"
...
void fsm_handle_event(const fsm_event_t *evt);
```
**Depois:** adicionado `#include <stdbool.h>` e protótipo:
```c
void fsm_set_player_at_equipment(bool at);
```

### `components/ui/include/ui_internal.h`
**Antes:** sem declarações de HUD nem constante de altura.
**Depois:** adicionado:
```c
#define UI_HUD_HEIGHT_PX  32
void screen_hud_build(lv_obj_t *parent);
void screen_hud_destroy(void);
void screen_hud_tick(void);
```

## Resultado
✅ Build passou. Boot OK até crash esperado no NFC (ver entrada T1900).

⚠️ Pode haver mais inconsistências escondidas em caminhos do código que ainda não foram exercitados. Quando o jogo avançar pra ramos novos (game over, room, etc.), podem aparecer mais.

## Links
- Entrada anterior: [[2026-05-26T1830-disable-psram]]
- Próxima entrada: [[2026-05-26T1900-nfc-tolerante]]
