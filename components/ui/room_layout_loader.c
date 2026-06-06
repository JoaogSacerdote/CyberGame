#include "room_layout_loader.h"

#include "room_layout.h"
#include "entity_pool.h"
#include "y_sort.h"
#include "asset_loader.h"
#include "game_config.h"   /* PLAYER_FRAME_W/H */
#include "esp_log.h"

static const char *TAG = "ROOM_LOADER";

size_t room_layout_spawn(lv_obj_t *parent, const char *room_name,
                         entity_t **out_player)
{
    if (out_player != NULL) {
        *out_player = NULL;
    }

    const room_layout_t *rl = room_layout_find(room_name);
    if (rl == NULL) {
        ESP_LOGW(TAG, "sala '%s' nao tem layout — nada a instanciar", room_name);
        return 0;
    }

    size_t spawned = 0;
    for (size_t i = 0; i < rl->count; ++i) {
        const room_entity_def_t *d = &rl->entities[i];

        /* 1. carrega o asset (tolerante: falha -> pula a entidade). */
        loaded_asset_t la;
        const esp_err_t err =
            asset_loader_load((asset_type_t)d->asset_type, d->asset_id, &la);
        if (err != ESP_OK) {
            ESP_LOGW(TAG, "'%s' asset (%u,%u) falhou (%s) — pulada",
                     d->name, d->asset_type, d->asset_id, esp_err_to_name(err));
            continue;
        }

        /* 2. aloca a entity no pool. */
        entity_t *e = entity_pool_alloc();
        if (e == NULL) {
            ESP_LOGE(TAG, "entity_pool cheio — '%s' nao instanciada", d->name);
            continue;
        }

        e->type     = d->type;
        e->flags    = d->flags;
        e->collision_offset_x = d->coll_offset_x;
        e->collision_offset_y = d->coll_offset_y;
        e->collision_w        = d->coll_w;
        e->collision_h        = d->coll_h;
        e->sort_offset_y      = d->sort_offset_y;

        /* 3. tamanho do sprite. O player usa um sheet (varios frames); o
         * tamanho visivel e UM frame (PLAYER_FRAME_*). Os demais usam o
         * tamanho real da imagem. */
        if (d->type == ENTITY_TYPE_PLAYER) {
            e->sprite_w = PLAYER_FRAME_W;
            e->sprite_h = PLAYER_FRAME_H;
        } else {
            e->sprite_w = la.dsc.header.w;
            e->sprite_h = la.dsc.header.h;
        }

        /* 4. posicao do pivot (base-centro). from_image: usa o offset do crop
         * (top-left no canvas original) convertido pra pivot. */
        if (d->from_image) {
            e->x = (int16_t)(la.off_x + e->sprite_w / 2);
            e->y = (int16_t)(la.off_y + e->sprite_h);
        } else {
            e->x = d->x;
            e->y = d->y;
        }

        /* 5. cria o lv_obj se for visivel (trigger pode ser zona invisivel).
         * IMPORTANTE: nao usar &la.dsc (variavel de pilha) como src — LVGL
         * guarda o ponteiro e dereferencia mais tarde, quando la ja saiu de
         * escopo. Usar o ponteiro persistente do cache via get_dsc(). */
        if (d->flags & ENTITY_FLAG_VISIBLE) {
            e->lv_obj = lv_image_create(parent);
            const lv_image_dsc_t *persistent = asset_loader_get_dsc(
                (asset_type_t)d->asset_type, d->asset_id);
            lv_image_set_src(e->lv_obj, persistent);
            lv_obj_remove_flag(e->lv_obj, LV_OBJ_FLAG_SCROLLABLE);
            if (d->type == ENTITY_TYPE_PLAYER) {
                /* janela de 1 frame; a tela anima via offset depois */
                lv_obj_set_size(e->lv_obj, PLAYER_FRAME_W, PLAYER_FRAME_H);
                lv_image_set_inner_align(e->lv_obj, LV_IMAGE_ALIGN_TOP_LEFT);
            }
            entity_apply_lv_pos(e);
        } else {
            e->lv_obj = NULL;
        }

        if (out_player != NULL && d->type == ENTITY_TYPE_PLAYER) {
            *out_player = e;
        }
        spawned++;
    }

    y_sort_mark_dirty();
    ESP_LOGI(TAG, "sala '%s': %u/%u entidades instanciadas",
             room_name, (unsigned)spawned, (unsigned)rl->count);
    return spawned;
}
