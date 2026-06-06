/* Gerado por tools/gen_room_layout.py — NAO EDITE A MAO.
 * Fonte: assets/layout/ENTIDADES.txt + POSICOES_*.txt */
#include "room_layout.h"
#include <string.h>

static const room_entity_def_t room_empresa_entities[] = {
    { .name="player_spawn", .asset_type=0, .asset_id=16, .type=ENTITY_TYPE_PLAYER, .flags=(ENTITY_FLAG_SOLID | ENTITY_FLAG_YSORTED | ENTITY_FLAG_VISIBLE), .x=56, .y=184, .coll_w=16, .coll_h=12, .coll_offset_x=-8, .coll_offset_y=-12, .sort_offset_y=0, .from_image=false },
    { .name="npc_ti_01", .asset_type=0, .asset_id=15, .type=ENTITY_TYPE_NPC, .flags=(ENTITY_FLAG_SOLID | ENTITY_FLAG_INTERACTABLE | ENTITY_FLAG_YSORTED | ENTITY_FLAG_VISIBLE), .x=0, .y=0, .coll_w=24, .coll_h=12, .coll_offset_x=-12, .coll_offset_y=-12, .sort_offset_y=0, .from_image=true },
};

static const room_entity_def_t room_recepcao_entities[] = {
    { .name="player_spawn", .asset_type=0, .asset_id=16, .type=ENTITY_TYPE_PLAYER, .flags=(ENTITY_FLAG_SOLID | ENTITY_FLAG_YSORTED | ENTITY_FLAG_VISIBLE), .x=240, .y=232, .coll_w=16, .coll_h=12, .coll_offset_x=-8, .coll_offset_y=-12, .sort_offset_y=0, .from_image=false },
    { .name="recepcionista_01", .asset_type=0, .asset_id=4, .type=ENTITY_TYPE_NPC, .flags=(ENTITY_FLAG_SOLID | ENTITY_FLAG_INTERACTABLE | ENTITY_FLAG_YSORTED | ENTITY_FLAG_VISIBLE), .x=0, .y=0, .coll_w=24, .coll_h=12, .coll_offset_x=-12, .coll_offset_y=-12, .sort_offset_y=0, .from_image=true },
};

const room_layout_t room_layouts[] = {
    { .room="empresa", .bg_asset_id=8, .entities=room_empresa_entities, .count=sizeof(room_empresa_entities)/sizeof(room_empresa_entities[0]) },
    { .room="recepcao", .bg_asset_id=0, .entities=room_recepcao_entities, .count=sizeof(room_recepcao_entities)/sizeof(room_recepcao_entities[0]) },
};
const size_t room_layouts_count = sizeof(room_layouts)/sizeof(room_layouts[0]);

const room_layout_t *room_layout_find(const char *room) {
    for (size_t i = 0; i < room_layouts_count; ++i)
        if (strcmp(room_layouts[i].room, room) == 0) return &room_layouts[i];
    return NULL;
}
