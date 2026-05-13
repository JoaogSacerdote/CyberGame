#include "ui.h"
#include "ui_internal.h"

#include "esp_log.h"
#include "lvgl.h"
#include "button_hal.h"

static const char *TAG = "UI_PAUSE";

static lv_obj_t      *s_root         = NULL;
static lv_timer_t    *s_timer        = NULL;
static button_state_t s_b_cache      = BTN_RELEASED;
static button_state_t s_start_cache  = BTN_RELEASED;

static void pause_tick(lv_timer_t *t)
{
    (void)t;
    /* START retoma — volta para gameplay (placeholder na Etapa A). */
    if (ui_btn_edge(BTN_START, &s_start_cache)) {
        ESP_LOGI(TAG, "START -> retoma placeholder");
        ui_show_placeholder();
        return;
    }
    /* B sai para o menu (descarta sessao). */
    if (ui_btn_edge(BTN_B, &s_b_cache)) {
        ESP_LOGI(TAG, "B -> menu (sai do gameplay)");
        ui_show_menu();
    }
}

void screen_pause_build(void)
{
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

    s_b_cache     = button_hal_peek(BTN_B);
    s_start_cache = button_hal_peek(BTN_START);
    s_timer = lv_timer_create(pause_tick, UI_TICK_MS, NULL);
    ESP_LOGI(TAG, "pause built");
}

void screen_pause_destroy(void)
{
    if (s_timer) { lv_timer_delete(s_timer); s_timer = NULL; }
    if (s_root)  { lv_obj_delete(s_root);    s_root  = NULL; }
}
