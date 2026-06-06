#pragma once

#include <stddef.h>
#include "lvgl.h"
#include "entity.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Instancia as entidades de uma sala a partir do layout gerado
 * (room_layouts[] em room_layout_gen.c). Para cada room_entity_def_t:
 *   - carrega o asset (asset_loader);
 *   - aloca entity_t no entity_pool;
 *   - cria lv_image filho de `parent` (se ENTITY_FLAG_VISIBLE);
 *   - preenche tipo/flags/colisao/tamanho/pivot e posiciona;
 *   - marca o Y-sort dirty no final.
 *
 * TOLERANTE: se um asset falhar (ex.: fundo grande sem PSRAM), pula aquela
 * entidade e segue — a sala sobe com o que couber na memoria.
 *
 * Chamar SOB lv_lock (mexe em lv_objs). Retorna quantas entidades subiram.
 * out_player (opcional) recebe a entity do player, ou NULL. */
size_t room_layout_spawn(lv_obj_t *parent, const char *room_name,
                         entity_t **out_player);

#ifdef __cplusplus
}
#endif
