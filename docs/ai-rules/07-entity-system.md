# 07 — Entity System (gameplay top-down 2D)

> Como modelar personagens, NPCs, móveis e props da sala do jogo.
> Compatível com a stack LVGL existente. Sem PSRAM (cabe em SRAM).

## Conceito central

Em jogo top-down 2D não existe Z real. A ilusão de profundidade vem de
**ordem de desenho**. Para isso funcionar, cada entidade carrega 4
informações *separadas*:

| campo | significa | exemplo (player 32×48) |
|---|---|---|
| `x, y` | posição no mundo, **ponto do chão** (centro dos pés) | `(120, 160)` |
| `sprite_w, sprite_h` | tamanho visual em pixels | `32, 48` |
| `collision_box` | retângulo pequeno na **base** (não sprite inteiro) | `12×8 na base` |
| `sort_y` | valor que decide ordem de desenho | `= y` (pivot bottom-center) |

Sem essa separação você cai em barreira invisível (colidir contra o ar
acima de uma mesa alta) ou inversão de profundidade (passar por trás
de cadeira e ela ficar embaixo).

## Decisão fechada: integração com LVGL

Mantemos LVGL como sistema de renderização. **Entity é wrapper de
`lv_obj_t`**. Z-ordering hierárquico via `lv_obj_move_foreground()`
(seção 10 de [[../../CyberGameCore/_AGENT/MOC_game_design|MOC_game_design]]).

Não vamos escrever sprite blitter próprio. Aproveitamos:
- calibração de cor já validada (`calibracao-cores-display.md` no vault)
- byte swap + R-boost em `hal_bridge`
- pipeline de sprites via `asset_loader` (NAND → `lv_img_set_src`)

## Struct Entity

```c
// components/gamestate/include/entity.h
#pragma once

#include <stdint.h>
#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    ENTITY_TYPE_PLAYER = 0,
    ENTITY_TYPE_NPC,
    ENTITY_TYPE_FURNITURE,
    ENTITY_TYPE_PROP,
    ENTITY_TYPE_TRIGGER,
    ENTITY_TYPE_MAX_COUNT,
} entity_type_t;

/* Flags bitmask — combinar com | */
#define ENTITY_FLAG_SOLID         (1u << 0)   /* bloqueia movimento */
#define ENTITY_FLAG_MOVABLE       (1u << 1)   /* pode ser empurrado/levantado */
#define ENTITY_FLAG_CARRYABLE     (1u << 2)   /* player pode segurar */
#define ENTITY_FLAG_INTERACTABLE  (1u << 3)   /* responde a botao A */
#define ENTITY_FLAG_TRIGGER       (1u << 4)   /* dispara evento ao colidir */
#define ENTITY_FLAG_VISIBLE       (1u << 5)   /* desenhada? (debug oculta) */
#define ENTITY_FLAG_YSORTED       (1u << 6)   /* participa do Y-sort */

typedef struct {
    uint32_t        id;
    entity_type_t   type;
    uint32_t        flags;

    /* Mundo (pixels). Pivot bottom-center. */
    int16_t         x;
    int16_t         y;

    /* Visual. */
    int16_t         sprite_w;
    int16_t         sprite_h;
    lv_obj_t       *lv_obj;          /* lv_image ou lv_obj container */

    /* Collision box: retangulo na BASE, offset do pivot. */
    int16_t         collision_offset_x;   /* tipicamente -w/2 */
    int16_t         collision_offset_y;   /* tipicamente -h ou -8 */
    int16_t         collision_w;
    int16_t         collision_h;

    /* Y-sort: tipicamente 0 (usa y direto). Negativo se a base do sprite
     * nao bate exatamente com (x,y). */
    int16_t         sort_offset_y;
} entity_t;

#ifdef __cplusplus
}
#endif
```

**Tamanho**: ~32 bytes/entity (com padding). 50 entities em uma sala = **1.6 KB**.
Cabe folgado em SRAM mesmo com PSRAM desabilitada.

## Pivot e desenho

Convenção: **pivot bottom-center**. `(x, y)` é o ponto que toca o chão.

Para desenhar o sprite na tela:
```c
draw_x_top_left = x - sprite_w / 2
draw_y_top_left = y - sprite_h
```

Em LVGL, isso significa ajustar a posição do `lv_obj` antes do
`lv_obj_set_pos`. Helper:

```c
static inline void entity_apply_lv_pos(const entity_t *e)
{
    lv_obj_set_pos(e->lv_obj,
                   e->x - e->sprite_w / 2,
                   e->y - e->sprite_h);
}
```

## sort_y

Para a maioria das entities com pivot bottom-center:
```c
sort_y = entity->y + entity->sort_offset_y;   // sort_offset_y geralmente 0
```

Quem tem maior `sort_y` é desenhado por cima. Em LVGL, isso é aplicado
via `lv_obj_move_foreground(e->lv_obj)` em ordem crescente de `sort_y`
(menor primeiro, maior por último). Detalhes em [08-y-sort-and-collision.md](08-y-sort-and-collision.md).

## Collision box

**Pequena na base**, não sprite inteiro. Resolve a sensação de barreira
invisível em sprites altos (mesa, player, NPC).

Tamanhos sugeridos por tipo:

| sprite | tamanho | collision (w × h) na base |
|---|---|---|
| Player 32×48 | 32×48 | **12×8** |
| Cadeira 16×16 | 16×16 | **8×6** |
| Mesa 32×32 | 32×32 | **24×8** |
| Mesa retangular 48×32 | 48×32 | **40×8** |
| Terminal 32×48 | 32×48 | **24×12** |
| Porta 32×48 | 32×48 | **28×4** |
| NPC 32×48 | 32×48 | **14×8** |

Para obter o retângulo absoluto em coords de mundo:

```c
static inline void entity_get_collision_rect(const entity_t *e,
                                             int16_t *x, int16_t *y,
                                             int16_t *w, int16_t *h)
{
    *x = e->x + e->collision_offset_x;
    *y = e->y + e->collision_offset_y;
    *w = e->collision_w;
    *h = e->collision_h;
}
```

## Ciclo de vida

### Criar entity

```c
entity_t *entity_create(entity_type_t type, int16_t x, int16_t y,
                        const char *sprite_path);
```

Implementação aloca a struct (de pool estática preferencialmente — ver
"Pool" abaixo), cria `lv_obj`, carrega sprite via `asset_loader_get(sprite_path)`,
posiciona via `entity_apply_lv_pos`.

### Destruir

```c
void entity_destroy(entity_t *e);
```

Chama `lv_obj_delete(e->lv_obj)`, libera slot do pool. **Sempre sob
`lv_lock`** (regra `feedback_lvgl_diff_gating`).

### Mover

```c
/* Tenta mover dx,dy. Faz separacao X/Y para colisao (ver 08). */
bool entity_try_move(entity_t *e, int16_t dx, int16_t dy);
```

Detalhe em [08-y-sort-and-collision.md](08-y-sort-and-collision.md) §movement.

## Pool estático em vez de heap

Para evitar fragmentação de heap no loop de jogo, **entities vivem em
pool estático** dimensionado por sala.

```c
// components/gamestate/entity_pool.c
#define ENTITY_POOL_CAPACITY  64    /* max entities por sala */

static entity_t s_pool[ENTITY_POOL_CAPACITY];
static bool     s_used[ENTITY_POOL_CAPACITY];
static size_t   s_count;
```

Custo: `64 × 32 = 2 KB` permanentes em BSS. Aceitável.

Trocar de sala = `entity_pool_clear()` (chama destroy em todas + zera
flags).

## Integração com FSM

A `Entity` é dado, não controla fluxo. Quem coordena é a FSM (ver
[02-fsm-pattern.md](02-fsm-pattern.md)). Padrão:

- FSM em `GAMEPLAY_SUB_EXPLORANDO`: lê input → `entity_try_move(player, dx, dy)` → roda Y-sort.
- FSM em `GAMEPLAY_SUB_TERMINAL_ABERTO`: player travado, terminal LVGL no foreground.
- Trocar de sala: `entity_pool_clear()` + `map_load_room(R)` + `entity_pool_repopulate_from(map)`.

## Regras

### R7.1 — Pivot SEMPRE bottom-center

Não inverter por sprite individual. Toda Entity assume essa convenção.
Se o sprite em PNG tiver pivot no centro, ajustar **na arte**, não no
código.

### R7.2 — `(x, y)` em **pixels do mundo**, não em tiles

Tile é só organização do mapa. Movimento é livre por pixel. Permite
movimento suave + colisão precisa.

### R7.3 — Collision box nunca igual ao sprite inteiro

Se você está tentado a setar `collision_w = sprite_w`, **pare**.
Significa que está modelando sprite como caixa sólida, e o jogo vai
parecer travado. Sempre menor, sempre na base.

### R7.4 — `lv_obj` só manipulado sob `lv_lock`

Toda alteração no `e->lv_obj` (mover, trocar src, deletar) é feita
dentro de `lv_lock()` / `lv_unlock()`. Regra geral do projeto.

### R7.5 — `id` é global e único na sessão

`id = 1, 2, 3, ...` atribuído por `entity_create`. Não reciclar IDs
durante uma sessão de gameplay. Player tem ID fixo `0`.

### R7.6 — Entity sem `lv_obj` é bug (exceto TRIGGER)

`ENTITY_TYPE_TRIGGER` pode ter `lv_obj == NULL` (são zonas invisíveis).
Outras flags assumem visualização — `assert(e->lv_obj != NULL)` no
`entity_apply_lv_pos`.

### R7.7 — Flags definem comportamento, não tipo

`type` é categoria (player/npc/furniture). `flags` é o que ela **faz**.
Uma cadeira normalmente é `SOLID | VISIBLE | YSORTED`. Uma carregável
ganha `CARRYABLE`. Misture.

## Anti-padrões

- ❌ Entity com `sprite_w == 0` (rascunho que nunca foi finalizado).
- ❌ Mexer em `e->lv_obj` fora de `lv_lock`.
- ❌ `entity_create` retornando ponteiro novo sem checar pool cheio.
- ❌ Player com `MOVABLE` (player se move por input direto, não por físico).
- ❌ Hardcoding de pixel positions em `screen_*.c` para gameplay — sempre via Entity.

## Próximos arquivos relacionados

- [08-y-sort-and-collision.md](08-y-sort-and-collision.md) — algoritmos
  de Y-sort (insertion sort) e AABB collision.
- [09-asset-pipeline.md](09-asset-pipeline.md) — como sprites e mapas
  chegam à NAND e são consumidos pela Entity.

## Referências externas

- Doc original do usuário ("Apanhado Completo: Sistema de Profundidade",
  2026-05-27). Adaptado para LVGL + sem PSRAM.
- `CyberGameCore/CONSULTA/Artigo.pdf` §3.2 (mapa top-down, setores).
- Pattern Y-sort: Stardew Valley, Pokémon — referência de mercado.
