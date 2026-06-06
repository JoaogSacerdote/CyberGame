#include "ui.h"
#include "ui_internal.h"

#include <stdio.h>
#include "lvgl.h"
#include "esp_log.h"
#include "gamestate.h"

static const char *TAG = "UI_HUD";

static lv_obj_t *s_root      = NULL;
static lv_obj_t *s_lbl_clock = NULL;
static lv_obj_t *s_lbl_vidas = NULL;

/* Diff-gating: so atualiza o label quando o valor muda. Evita
 * lv_label_set_text desnecessario por tick (~10 vezes por segundo). */
static uint16_t  s_last_minutes = 0xFFFF;
static uint8_t   s_last_vidas   = 0xFF;

void screen_hud_build(lv_obj_t *parent)
{
    if (s_root) {
        /* Build duplicado — ignora pra nao vazar objetos. */
        return;
    }

    s_root = lv_obj_create(parent);
    lv_obj_set_size(s_root, 480, UI_HUD_HEIGHT_PX);
    lv_obj_set_pos(s_root, 0, 0);
    lv_obj_set_style_bg_color(s_root, lv_color_hex(0x101418), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(s_root, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_color(s_root, lv_color_hex(0x303840), LV_PART_MAIN);
    lv_obj_set_style_border_width(s_root, 1, LV_PART_MAIN);
    lv_obj_set_style_border_side(s_root, LV_BORDER_SIDE_BOTTOM, LV_PART_MAIN);
    lv_obj_set_style_pad_all(s_root, 0, LV_PART_MAIN);
    lv_obj_remove_flag(s_root, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scroll_dir(s_root, LV_DIR_NONE);

    s_lbl_clock = lv_label_create(s_root);
    lv_label_set_text(s_lbl_clock, "08:00");
    lv_obj_set_style_text_color(s_lbl_clock, lv_color_hex(0xE6E6E6), LV_PART_MAIN);
    lv_obj_align(s_lbl_clock, LV_ALIGN_LEFT_MID, 8, 0);

    s_lbl_vidas = lv_label_create(s_root);
    lv_label_set_text(s_lbl_vidas, "Vidas: 3");
    lv_obj_set_style_text_color(s_lbl_vidas, lv_color_hex(0xE6E6E6), LV_PART_MAIN);
    lv_obj_align(s_lbl_vidas, LV_ALIGN_RIGHT_MID, -8, 0);

    s_last_minutes = 0xFFFF; /* forca primeiro update */
    s_last_vidas   = 0xFF;
    ESP_LOGI(TAG, "hud built");
}

void screen_hud_destroy(void)
{
    if (s_root) {
        lv_obj_delete(s_root);
        s_root      = NULL;
        s_lbl_clock = NULL;
        s_lbl_vidas = NULL;
    }
}

void screen_hud_tick(void)
{
    if (!s_root) return;

    /* Clock (HH:MM) */
    if (s_lbl_clock) {
        const uint16_t mins = gamestate_get_clock_minutes();
        if (mins != s_last_minutes) {
            s_last_minutes = mins;
            char buf[8];
            snprintf(buf, sizeof(buf), "%02u:%02u",
                     (unsigned)(mins / 60), (unsigned)(mins % 60));
            lv_label_set_text(s_lbl_clock, buf);
        }
    }

    /* Vidas */
    if (s_lbl_vidas) {
        const uint8_t v = gamestate_get_vidas();
        if (v != s_last_vidas) {
            s_last_vidas = v;
            char buf[12];
            snprintf(buf, sizeof(buf), "Vidas: %u", (unsigned)v);
            lv_label_set_text(s_lbl_vidas, buf);
        }
    }
}
