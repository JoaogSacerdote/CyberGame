#pragma once

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include "entity.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Definicao ESTATICA de uma entidade numa sala — gerada por
 * tools/gen_room_layout.py a partir de assets/room_layout.json.
 *
 * NAO confundir com entity_t (estado vivo, com lv_obj). Isto e o "molde":
 * o loader (na camada de UI) le um room_entity_def_t, aloca uma entity_t no
 * pool, carrega o asset e posiciona. Por isso aqui guardamos so dados leves
 * (ids numericos, sem ponteiros LVGL) — cabe na flash sem pesar. */
typedef struct {
    const char    *name;          /* rotulo humano (debug) */
    uint8_t        asset_type;    /* asset_type_t (0 = sprite) */
    uint16_t       asset_id;      /* id no asset_registry.json */
    entity_type_t  type;          /* player/npc/furniture/prop/trigger */
    uint32_t       flags;         /* ENTITY_FLAG_* combinadas */

    int16_t        x;             /* pivot base-centro (px). Ignorado se from_image. */
    int16_t        y;

    int16_t        coll_w;        /* caixa de colisao na base */
    int16_t        coll_h;
    int16_t        coll_offset_x;
    int16_t        coll_offset_y;

    int16_t        sort_offset_y; /* ajuste do Y-sort (quase sempre 0) */
    bool           from_image;    /* true -> usa o offset do crop do asset como x/y */
} room_entity_def_t;

/* Uma sala: nome, asset de fundo (piso) e o array de entidades. */
typedef struct {
    const char              *room;
    uint16_t                 bg_asset_id;
    const room_entity_def_t *entities;
    size_t                   count;
} room_layout_t;

/* Tabela gerada (room_layout_gen.c). Iterar com room_layouts_count. */
extern const room_layout_t room_layouts[];
extern const size_t        room_layouts_count;

/* Acha o layout de uma sala pelo nome, ou NULL. Definido em room_layout_gen.c. */
const room_layout_t *room_layout_find(const char *room);

#ifdef __cplusplus
}
#endif
