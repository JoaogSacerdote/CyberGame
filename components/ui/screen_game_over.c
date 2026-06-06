#include "ui.h"
#include "ui_internal.h"

#include "esp_log.h"
#include "lvgl.h"

static const char *TAG = "UI_GAMEOVER";

static lv_obj_t *s_root = NULL;

/* Placeholder. Quando os assets da Tela de Demissao chegarem (SD card), trocar
 * os labels por lv_image_create() com a sprite editada. */

void screen_game_over_build(void)
{
    s_root = lv_obj_create(lv_screen_active());
    lv_obj_set_size(s_root, 480, 320);
    lv_obj_set_pos(s_root, 0, 0);
    lv_obj_set_style_bg_color(s_root, lv_color_hex(0x2A0810), LV_PART_MAIN); /* vinho */
    lv_obj_set_style_bg_opa(s_root, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(s_root, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(s_root, 0, LV_PART_MAIN);
    lv_obj_remove_flag(s_root, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scroll_dir(s_root, LV_DIR_NONE);

    lv_obj_t *title = lv_label_create(s_root);
    lv_label_set_text(title, "DEMITIDO");
    lv_obj_set_style_text_color(title, lv_color_hex(0xFF4040), LV_PART_MAIN);
    lv_obj_align(title, LV_ALIGN_CENTER, 0, -40);

    lv_obj_t *sub = lv_label_create(s_root);
    lv_label_set_text(sub, "Voce esgotou suas vidas.");
    lv_obj_set_style_text_color(sub, UI_COLOR_TEXT, LV_PART_MAIN);
    lv_obj_align(sub, LV_ALIGN_CENTER, 0, 0);

    lv_obj_t *hint = lv_label_create(s_root);
    lv_label_set_text(hint, "[A] Tentar Novamente    [B] Menu");
    lv_obj_set_style_text_color(hint, UI_COLOR_DIM, LV_PART_MAIN);
    lv_obj_align(hint, LV_ALIGN_CENTER, 0, 40);

    ESP_LOGI(TAG, "game over built");
}

void screen_game_over_destroy(void)
{
    if (s_root) { lv_obj_delete(s_root); s_root = NULL; }
}
