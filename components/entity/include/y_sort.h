#pragma once

#include <stdbool.h>
#include "entity.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Inicializa estado. Idempotente. Marca dirty para forcar primeiro sort. */
void y_sort_init(void);

/* Sinaliza que a ordem precisa ser recalculada no proximo run. Chamado
 * por entity_try_move e por callers que mexem em entity->y diretamente. */
void y_sort_mark_dirty(void);

/* Inspeciona flag sem alterar. */
bool y_sort_is_dirty(void);

/* Re-ordena entities visiveis com FLAG_YSORTED por sort_y crescente
 * (insertion sort, ideal para listas quase-ordenadas que e o caso
 * comum entre frames) e aplica a ordem na arvore LVGL via
 * lv_obj_move_foreground. Limpa o dirty flag ao final.
 *
 * Chamar SOB lv_lock. No-op se !y_sort_is_dirty(). */
void y_sort_run(void);

/* Coordena render por frame: lv_lock + y_sort_run (no-op se !dirty) +
 * debug_overlay_redraw (no-op se !enabled) + lv_unlock.
 *
 * Engine chama sem precisar incluir lvgl.h nem saber sobre lock.
 * Idempotente; barato em frames sem mudanca (early-returns aninhados). */
void entity_render_sync(void);

#ifdef __cplusplus
}
#endif
