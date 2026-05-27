#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Tipos de entidades do gameplay top-down. Categoria, nao comportamento.
 * Comportamento eh definido por flags abaixo. */
typedef enum {
    ENTITY_TYPE_PLAYER = 0,
    ENTITY_TYPE_NPC,
    ENTITY_TYPE_FURNITURE,
    ENTITY_TYPE_PROP,
    ENTITY_TYPE_TRIGGER,
    ENTITY_TYPE_MAX_COUNT,
} entity_type_t;

/* Flags bitmask — combinar com |. Definem o que a entity FAZ. */
#define ENTITY_FLAG_SOLID         (1u << 0)   /* bloqueia movimento de outras entities */
#define ENTITY_FLAG_MOVABLE       (1u << 1)   /* pode ser empurrado/levantado */
#define ENTITY_FLAG_CARRYABLE     (1u << 2)   /* player pode segurar e mover junto */
#define ENTITY_FLAG_INTERACTABLE  (1u << 3)   /* responde a botao A quando perto */
#define ENTITY_FLAG_TRIGGER       (1u << 4)   /* dispara evento ao colidir com player */
#define ENTITY_FLAG_VISIBLE       (1u << 5)   /* desenhada na tela (debug pode ocultar) */
#define ENTITY_FLAG_YSORTED       (1u << 6)   /* participa do Y-sort dinamico */

/* Struct principal. Pivot bottom-center: (x, y) = centro dos pes / base
 * que toca o chao. Ver docs/ai-rules/07-entity-system.md. */
typedef struct {
    uint32_t        id;                    /* unico na sessao; player = 0 */
    entity_type_t   type;
    uint32_t        flags;

    /* Posicao no mundo, em pixels. Pivot bottom-center. */
    int16_t         x;
    int16_t         y;

    /* Tamanho visual do sprite. */
    int16_t         sprite_w;
    int16_t         sprite_h;

    /* LVGL object que renderiza esta entity (lv_image ou container).
     * NULL valido apenas em ENTITY_TYPE_TRIGGER (zona invisivel). */
    lv_obj_t       *lv_obj;

    /* Collision box: retangulo na BASE, offsets aplicados ao pivot (x,y).
     * Pequeno por design — evita barreira invisivel em sprites altos. */
    int16_t         collision_offset_x;    /* tipicamente -collision_w / 2 */
    int16_t         collision_offset_y;    /* tipicamente -collision_h (na base) */
    int16_t         collision_w;
    int16_t         collision_h;

    /* Ajuste fino do sort_y. Tipicamente 0 (usa y direto). Negativo se a
     * base visual do sprite nao coincide com o pivot. */
    int16_t         sort_offset_y;
} entity_t;

/* Helpers inline (puro, sem state — safe em qualquer contexto). */

static inline int16_t entity_sort_y(const entity_t *e)
{
    return e->y + e->sort_offset_y;
}

static inline void entity_get_collision_rect(const entity_t *e,
                                             int16_t *out_x,
                                             int16_t *out_y,
                                             int16_t *out_w,
                                             int16_t *out_h)
{
    *out_x = e->x + e->collision_offset_x;
    *out_y = e->y + e->collision_offset_y;
    *out_w = e->collision_w;
    *out_h = e->collision_h;
}

/* Aplica posicao do pivot (x,y) ao lv_obj. Top-left do sprite calculado
 * a partir do pivot bottom-center. Chamar SOB lv_lock. No-op se sem lv_obj. */
static inline void entity_apply_lv_pos(entity_t *e)
{
    if (e->lv_obj != NULL) {
        lv_obj_set_pos(e->lv_obj,
                       e->x - e->sprite_w / 2,
                       e->y - e->sprite_h);
    }
}

#ifdef __cplusplus
}
#endif
