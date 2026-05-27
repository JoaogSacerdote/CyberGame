# 08 — Y-sort, colisão AABB, camadas e debug visual

> Algoritmos que fazem o gameplay top-down funcionar. Aplicação direta:
> sala `Recepção`, `Empresa`, salas futuras com [[entity-system|entities]].

## Camadas (Z-ordering hierárquico)

Quatro camadas LVGL, top-to-bottom:

| camada | conteúdo | colisão | Y-sort? |
|---|---|---|---|
| **Overlay_Front** | topo de parede, beirais, copa de árvore | ❌ | ❌ (sempre na frente) |
| **Objects_YSort** | player, NPCs, móveis, props | ✅ | ✅ (esta é a layer dinâmica) |
| **Walls_Back** | paredes vistas de frente, fundo de cenário | ✅ (estática) | ❌ (sempre atrás dos Objects) |
| **Ground** | chão, piso, carpete, marcas | ❌ | ❌ (sempre no fundo) |

Em LVGL: 4 `lv_obj` containers como filhos da screen, criados nessa
ordem. Como LVGL desenha pais antes de filhos e em ordem de
criação, `Ground` fica embaixo, `Overlay_Front` em cima.

```c
// pseudocodigo de inicializacao de sala
lv_obj_t *layer_ground   = lv_obj_create(screen);
lv_obj_t *layer_walls    = lv_obj_create(screen);
lv_obj_t *layer_ysort    = lv_obj_create(screen);
lv_obj_t *layer_overlay  = lv_obj_create(screen);
```

Entities `YSORTED` viram filhas de `layer_ysort`.

## Y-sort: algoritmo

Dentro de `layer_ysort`, a **ordem dos filhos** define quem fica na
frente (último filho = mais visível). Refazemos essa ordem a cada
frame *quando alguma entity moveu*.

### Algoritmo: insertion sort

Lista de entities geralmente está **quase ordenada** entre frames
(entities movem pouco). Insertion sort é ideal:
- O(N) no melhor caso (lista já ordenada)
- O(N²) no pior, mas pior caso aqui é raro
- Custo fixo baixíssimo
- Implementação em ~10 linhas

`qsort()` da libc é genérico (overhead de function pointer + swap por
indireção) e seu pior caso pode ser O(N²) também. **Insertion sort é
mais rápido para N pequeno e quase ordenado.**

```c
// components/gamestate/y_sort.c

static entity_t *s_sorted[ENTITY_POOL_CAPACITY];  // ponteiros, não copias
static size_t    s_sorted_count;

static int16_t entity_sort_y(const entity_t *e)
{
    return e->y + e->sort_offset_y;
}

void y_sort_run(void)
{
    /* Insertion sort: cresce a parte ordenada [0..i-1] adicionando s_sorted[i].
     * Lista quase-ordenada -> swap loop quase nunca executa. */
    for (size_t i = 1; i < s_sorted_count; ++i) {
        entity_t *cur     = s_sorted[i];
        const int16_t key = entity_sort_y(cur);
        size_t j = i;
        while (j > 0 && entity_sort_y(s_sorted[j - 1]) > key) {
            s_sorted[j] = s_sorted[j - 1];
            --j;
        }
        s_sorted[j] = cur;
    }

    /* Aplica ordem na arvore LVGL: lv_obj_move_foreground na ordem.
     * Quem chama por ultimo fica mais no topo. */
    for (size_t i = 0; i < s_sorted_count; ++i) {
        if (s_sorted[i]->lv_obj) {
            lv_obj_move_foreground(s_sorted[i]->lv_obj);
        }
    }
}
```

**Custo medido esperado**: 30 entities × ~5 comparações cada = ~150
comparações por frame. Em ESP32-S3 240 MHz isso é trivial (<1 μs).

### Quando chamar `y_sort_run`?

- A cada frame onde **alguma entity moveu** (rastrear via flag
  `s_ysort_dirty`).
- Após criar/destruir entity (adiciona/remove de `s_sorted`).

Não chamar a 60 Hz cego — só quando dirty. Em frame sem movimento,
zero custo.

## Colisão AABB com separação X/Y

### Padrão

Para movimento de player/NPC, **separar movimento em 2 passos** (X
primeiro, depois Y). Isso permite deslizar contra paredes em vez de
travar.

```c
// components/gamestate/movement.c

bool entity_try_move(entity_t *e, int16_t dx, int16_t dy)
{
    bool moved = false;

    /* Tenta X. */
    if (dx != 0) {
        e->x += dx;
        if (entity_collides_solid(e)) {
            e->x -= dx;   /* desfaz */
        } else {
            moved = true;
        }
    }

    /* Tenta Y. */
    if (dy != 0) {
        e->y += dy;
        if (entity_collides_solid(e)) {
            e->y -= dy;
        } else {
            moved = true;
        }
    }

    if (moved) {
        entity_apply_lv_pos(e);
        s_ysort_dirty = true;
    }
    return moved;
}
```

Sem separação X/Y: bater diagonal em parede trava player completo.
Com: player desliza pela parede.

### `entity_collides_solid` — busca linear (POR HORA)

Para salas com ≤50 entities sólidas (que é nosso caso), busca linear
é mais rápida que quadtree:

```c
static bool aabb_intersect(int16_t ax, int16_t ay, int16_t aw, int16_t ah,
                           int16_t bx, int16_t by, int16_t bw, int16_t bh)
{
    return !(ax + aw <= bx || bx + bw <= ax ||
             ay + ah <= by || by + bh <= ay);
}

bool entity_collides_solid(const entity_t *self)
{
    int16_t ax, ay, aw, ah;
    entity_get_collision_rect(self, &ax, &ay, &aw, &ah);

    for (size_t i = 0; i < entity_pool_count(); ++i) {
        entity_t *other = entity_pool_at(i);
        if (other == self) continue;
        if (!(other->flags & ENTITY_FLAG_SOLID)) continue;

        int16_t bx, by, bw, bh;
        entity_get_collision_rect(other, &bx, &by, &bw, &bh);
        if (aabb_intersect(ax, ay, aw, ah, bx, by, bw, bh)) {
            return true;
        }
    }
    return false;
}
```

Quando passar de 50 entities por sala, considerar grid espacial. **Não
prematuramente.**

### Colisão contra tilemap

Algumas células do tilemap são sólidas (paredes que ocupam um tile
inteiro). Em vez de criar entity para cada, ter uma **layer de colisão**
no tilemap:

```c
// components/gamestate/tilemap.h
typedef enum {
    TILE_PASSABLE = 0,
    TILE_SOLID    = 1,
} tile_collision_t;

static uint8_t s_collision_map[MAP_TILES_W * MAP_TILES_H];
```

Adicionar check no `entity_collides_solid`:

```c
/* Verifica colisao contra tilemap. Para cada tile que o collision_box
 * sobrepoe, ver se eh TILE_SOLID. */
const int16_t tx0 = ax / TILE_SIZE;
const int16_t ty0 = ay / TILE_SIZE;
const int16_t tx1 = (ax + aw - 1) / TILE_SIZE;
const int16_t ty1 = (ay + ah - 1) / TILE_SIZE;
for (int16_t ty = ty0; ty <= ty1; ++ty) {
    for (int16_t tx = tx0; tx <= tx1; ++tx) {
        if (s_collision_map[ty * MAP_TILES_W + tx] == TILE_SOLID) {
            return true;
        }
    }
}
```

## Tilemap — formato em memória

Para sala 30×20 tiles (480×320 com tile 16):

```c
// 4 layers de uint16_t (tile ID) — tilemap.c
static uint16_t s_layer_ground   [MAP_TILES_W * MAP_TILES_H];   // 1.2 KB
static uint16_t s_layer_walls    [MAP_TILES_W * MAP_TILES_H];
static uint16_t s_layer_overlay  [MAP_TILES_W * MAP_TILES_H];

// 1 layer de colisão (bool empacotado em uint8_t)
static uint8_t  s_collision_map  [MAP_TILES_W * MAP_TILES_H];   // 600 B
```

Total: `1200 × 3 + 600 = ~4.2 KB` em BSS. Cabe em SRAM mesmo com 50
entities (1.6 KB) + pool overhead.

Tile ID `0` = vazio (não desenhar).

Render de tilemap em LVGL: pré-criar `lv_obj` para cada tile não-zero
da layer Ground/Walls/Overlay no carregamento da sala (já que tiles
fixos não mudam). Não usa o Y-sort. Mantém ordem de pais/filhos.

## Modo debug visual

Toggle por compile-time ou runtime. Quando ativo, sobre o renderer
normal, desenha:

| visual | o quê |
|---|---|
| Retângulo **verde** outline | `sprite_w × sprite_h` (área visual completa) |
| Retângulo **vermelho** filled | `collision_box` (caixa da base) |
| Ponto **amarelo** | pivot `(x, y)` |
| Linha **azul** horizontal | `sort_y` na largura da entity |
| Texto **branco** | `type:ID` |

Implementação: criar uma layer `Debug_Overlay` acima de `Overlay_Front`,
com canvas LVGL. Limpar e redesenhar a cada frame quando dirty.

```c
// components/gamestate/debug_overlay.c
void debug_overlay_set_enabled(bool en);
bool debug_overlay_is_enabled(void);
void debug_overlay_redraw(void);   /* chamado quando ysort_dirty */
```

Toggle: combo de botões (sugerido `Y + B` por 2s) ou flag no
sdkconfig.

## Regras

### R8.1 — Y-sort só quando dirty

```c
if (s_ysort_dirty) {
    y_sort_run();
    s_ysort_dirty = false;
}
```

Não roda em frame parado. Performance grátis.

### R8.2 — Movement de player passa SEMPRE por `entity_try_move`

Nunca `player->x += dx` direto em `screen_empresa.c` ou afins. Sem
isso colisão é byass-able por bug ou intent.

### R8.3 — `collision_box` deve estar dentro do `sprite_box`

Garantir que `collision_box` (offset + size) cabe dentro do retângulo
visual. Asserção em debug:

```c
assert(e->collision_offset_x + e->collision_w <= e->sprite_w / 2);
assert(e->collision_offset_y + e->collision_h <= 0);  /* base, nao topo */
```

### R8.4 — Tilemap não muda em runtime

Tile changes (porta que abre, parede que cai) viram **entities**, não
edições de tilemap. Tilemap é cenário estático carregado uma vez por sala.

### R8.5 — Camadas LVGL fixas

Ordem `Ground → Walls_Back → Y-Sort → Overlay_Front` nunca é
alterada. Trocar de sala destrói e recria as 4 layers nessa ordem.

### R8.6 — Insertion sort sem libs

Não importar `qsort()`. Manter inline em `y_sort.c`. Implementação cabe
em 15 linhas.

### R8.7 — Trigger não tem collision sólida

`ENTITY_FLAG_TRIGGER` ≠ `ENTITY_FLAG_SOLID`. Triggers detectam
sobreposição mas não bloqueiam. Implementar busca separada:

```c
void trigger_check_overlaps(entity_t *player);
/* Para cada entity com FLAG_TRIGGER, se aabb_intersect(player, trigger),
 * postar FSM_EVT_TRIGGER_ENTER (uma vez na entrada). */
```

## Anti-padrões

- ❌ Usar `qsort()` para Y-sort.
- ❌ Roda Y-sort a 60 Hz mesmo sem movimento.
- ❌ Tentar movimento em uma única operação (sem separação X/Y).
- ❌ Colisão verificada contra `sprite_w × sprite_h` (barreira invisível).
- ❌ Tilemap modificado em runtime.
- ❌ Player com colisão circular (todos AABB nesse projeto).
- ❌ Y-sort fora da layer `Objects_YSort`.

## Próximos arquivos relacionados

- [07-entity-system.md](07-entity-system.md) — struct Entity, pool, ciclo
  de vida.
- [09-asset-pipeline.md](09-asset-pipeline.md) — como tilemap chega ao
  firmware (pré-compilado, não JSON em runtime).

## Referências externas

- Doc original do usuário ("Apanhado Completo: Sistema de Profundidade").
- Insertion sort em arrays quase-ordenados: Sedgewick, *Algorithms*.
- LVGL `lv_obj_move_foreground`: docs LVGL 9.x.
- `CyberGameCore/CONSULTA/Artigo.pdf` §3.2 — mapa top-down e setores.
- Memória `pending_visual_logic_refactor` no vault — debt visual a pagar
  com este sistema novo.
