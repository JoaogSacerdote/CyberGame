#include "debug_overlay.h"

#include "entity_pool.h"
#include "esp_log.h"

static const char *TAG = "DEBUG_OVERLAY";

/* Cores em RGB. LVGL converte conforme color depth. */
#define COLOR_SPRITE_OUTLINE     0x00FF00   /* verde puro */
#define COLOR_COLLISION_SOLID    0xFF0000   /* vermelho */
#define COLOR_COLLISION_TRIGGER  0xFFAA00   /* laranja (distingue de SOLID) */
#define COLOR_PIVOT_DOT          0xFFFF00   /* amarelo */
#define COLOR_SORT_LINE          0x00AAFF   /* azul claro */

#define PIVOT_DOT_SIZE           3
#define SORT_LINE_HALF_WIDTH     10          /* segmento horizontal de 20 px */
#define SORT_LINE_THICKNESS      1

static lv_obj_t *s_layer;
static bool      s_enabled;

static void make_unstyled(lv_obj_t *o)
{
    lv_obj_remove_style_all(o);
    lv_obj_clear_flag(o, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag(o, LV_OBJ_FLAG_CLICKABLE);
}

void debug_overlay_init(lv_obj_t *parent)
{
    if (s_layer != NULL) {
        return;       /* idempotente */
    }
    if (parent == NULL) {
        ESP_LOGE(TAG, "init com parent NULL");
        return;
    }

    s_layer = lv_obj_create(parent);
    make_unstyled(s_layer);
    lv_obj_set_size(s_layer, LV_PCT(100), LV_PCT(100));
    lv_obj_set_pos(s_layer, 0, 0);
    lv_obj_add_flag(s_layer, LV_OBJ_FLAG_HIDDEN);
    s_enabled = false;

    ESP_LOGI(TAG, "init OK");
}

void debug_overlay_set_enabled(bool en)
{
    s_enabled = en;
    if (s_layer == NULL) {
        ESP_LOGW(TAG, "set_enabled(%d) sem init previo", en);
        return;
    }
    if (en) {
        lv_obj_clear_flag(s_layer, LV_OBJ_FLAG_HIDDEN);
        lv_obj_move_foreground(s_layer);     /* sempre por cima */
    } else {
        lv_obj_add_flag(s_layer, LV_OBJ_FLAG_HIDDEN);
    }
}

bool debug_overlay_is_enabled(void)
{
    return s_enabled;
}

/* === helpers de desenho ===
 * Cada marcador eh um lv_obj filho de s_layer com estilo minimo. */

static void draw_rect_outline(int16_t x, int16_t y, int16_t w, int16_t h,
                              uint32_t color_hex)
{
    lv_obj_t *r = lv_obj_create(s_layer);
    make_unstyled(r);
    lv_obj_set_pos(r, x, y);
    lv_obj_set_size(r, w, h);
    lv_obj_set_style_bg_opa(r, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_color(r, lv_color_hex(color_hex), 0);
    lv_obj_set_style_border_width(r, 1, 0);
    lv_obj_set_style_radius(r, 0, 0);
}

static void draw_rect_filled(int16_t x, int16_t y, int16_t w, int16_t h,
                             uint32_t color_hex, lv_opa_t opa)
{
    lv_obj_t *r = lv_obj_create(s_layer);
    make_unstyled(r);
    lv_obj_set_pos(r, x, y);
    lv_obj_set_size(r, w, h);
    lv_obj_set_style_bg_color(r, lv_color_hex(color_hex), 0);
    lv_obj_set_style_bg_opa(r, opa, 0);
    lv_obj_set_style_border_width(r, 0, 0);
    lv_obj_set_style_radius(r, 0, 0);
}

void debug_overlay_redraw(void)
{
    if (!s_enabled || s_layer == NULL) return;

    /* Estrategia "recreate per frame": destroi todos os filhos antigos
     * e cria do zero. Para ~30 entities = ~120 lv_objs criados/frame.
     * Custo na ordem de ~1 ms — aceitavel em 30 FPS (33 ms/frame). */
    lv_obj_clean(s_layer);

    const size_t n = entity_pool_count();
    for (size_t i = 0; i < n; ++i) {
        entity_t *e = entity_pool_at(i);
        if (e == NULL) continue;

        /* 1. Sprite outline VERDE (apenas se VISIBLE) */
        if (e->flags & ENTITY_FLAG_VISIBLE) {
            draw_rect_outline(e->x - e->sprite_w / 2,
                              e->y - e->sprite_h,
                              e->sprite_w, e->sprite_h,
                              COLOR_SPRITE_OUTLINE);
        }

        /* 2. Collision box (vermelho para SOLID, laranja para TRIGGER).
         *    Ambos podem coexistir — desenhamos por cima sem fundir. */
        int16_t cx, cy, cw, ch;
        if (e->flags & ENTITY_FLAG_SOLID) {
            entity_get_collision_rect(e, &cx, &cy, &cw, &ch);
            draw_rect_filled(cx, cy, cw, ch, COLOR_COLLISION_SOLID, LV_OPA_30);
        }
        if (e->flags & ENTITY_FLAG_TRIGGER) {
            entity_get_collision_rect(e, &cx, &cy, &cw, &ch);
            draw_rect_filled(cx, cy, cw, ch, COLOR_COLLISION_TRIGGER, LV_OPA_30);
        }

        /* 3. Pivot AMARELO — ponto 3x3 centrado em (x, y). */
        draw_rect_filled(e->x - PIVOT_DOT_SIZE / 2,
                         e->y - PIVOT_DOT_SIZE / 2,
                         PIVOT_DOT_SIZE, PIVOT_DOT_SIZE,
                         COLOR_PIVOT_DOT, LV_OPA_COVER);

        /* 4. Sort_y AZUL — segmento horizontal pequeno em torno do pivot.
         *    So para YSORTED (a linha so importa para o sistema de Y-sort). */
        if (e->flags & ENTITY_FLAG_YSORTED) {
            const int16_t sy = entity_sort_y(e);
            draw_rect_filled(e->x - SORT_LINE_HALF_WIDTH,
                             sy,
                             SORT_LINE_HALF_WIDTH * 2,
                             SORT_LINE_THICKNESS,
                             COLOR_SORT_LINE, LV_OPA_70);
        }
    }
}
