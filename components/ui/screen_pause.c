#include "ui.h"
#include "ui_internal.h"

#include "esp_log.h"
#include "lvgl.h"

static const char *TAG = "UI_PAUSE";

static lv_obj_t *s_root = NULL;

/* PAUSE nao tem peek nem timer: a FSM consome START/B via queue e dispara
 * a transicao macro, e o engine_task troca a tela. Tela puramente estatica. */

void screen_pause_build(void)
{
    if (s_root) return;   /* overlay ja aberto */
    s_root = lv_obj_create(lv_screen_active());
    lv_obj_set_size(s_root, 480, 320);
    lv_obj_set_pos(s_root, 0, 0);
    /* Fundo escurecido (overlay), nao opaco total — sugere "por cima do jogo". */
    lv_obj_set_style_bg_color(s_root, lv_color_hex(0x000000), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(s_root, LV_OPA_70, LV_PART_MAIN);
    lv_obj_set_style_border_width(s_root, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(s_root, 0, LV_PART_MAIN);
    lv_obj_remove_flag(s_root, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *title = lv_label_create(s_root);
    lv_label_set_text(title, "PAUSADO");
    lv_obj_set_style_text_color(title, UI_COLOR_TEXT, LV_PART_MAIN);
    lv_obj_align(title, LV_ALIGN_CENTER, 0, -30);

    lv_obj_t *hint = lv_label_create(s_root);
    lv_label_set_text(hint, "START = continuar    B = sair");
    lv_obj_set_style_text_color(hint, UI_COLOR_DIM, LV_PART_MAIN);
    lv_obj_align(hint, LV_ALIGN_CENTER, 0, 30);

    ESP_LOGI(TAG, "pause built");
}

void screen_pause_destroy(void)
{
    if (s_root) { lv_obj_delete(s_root); s_root = NULL; }
}
