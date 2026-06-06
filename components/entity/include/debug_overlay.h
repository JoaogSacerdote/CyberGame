#pragma once

#include <stdbool.h>
#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Overlay visual de debug para o sistema de entities. Quando habilitado,
 * desenha sobre todo o gameplay:
 *
 *   - retangulo VERDE outline = sprite_w x sprite_h (area visual)
 *   - retangulo VERMELHO semi-transp = collision_box (SOLID)
 *   - retangulo LARANJA semi-transp = collision_box (TRIGGER)
 *   - ponto AMARELO 3x3 = pivot (x, y)
 *   - segmento AZUL horizontal = sort_y (so para entities YSORTED)
 *
 * Use para acertar visualmente colision boxes e profundidade ao
 * adicionar/posicionar entities novas. NAO deixar habilitado em release. */

/* Cria a camada de overlay como filha de 'parent' (tipicamente a screen
 * do gameplay). Idempotente. Comeca DESABILITADA e oculta.
 * Chamar sob lv_lock. */
void debug_overlay_init(lv_obj_t *parent);

/* Destroi a camada (caso a screen pai va morrer). set_enabled(true) volta
 * a auto-inicializar na screen ativa. Chamar sob lv_lock. */
void debug_overlay_deinit(void);

/* Liga/desliga visibilidade. Quando liga, auto-inicializa em
 * lv_screen_active() se ainda nao havia init + move_foreground. Quando
 * desliga, apenas oculta. Chamar sob lv_lock. */
void debug_overlay_set_enabled(bool en);

bool debug_overlay_is_enabled(void);

/* Re-desenha todos os marcadores baseado no estado atual do entity_pool.
 * Limpa e recria — sem state cross-frame.
 *
 * Chamar SOB lv_lock, idealmente quando y_sort_is_dirty() OR enabled-just-toggled.
 * No-op se !is_enabled(). */
void debug_overlay_redraw(void);

#ifdef __cplusplus
}
#endif
