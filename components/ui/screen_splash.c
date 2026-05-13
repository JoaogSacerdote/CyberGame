#include "ui.h"
#include "ui_internal.h"

#include "esp_log.h"
#include "lvgl.h"
#include "button_hal.h"
#include "fsm.h"

static const char *TAG = "UI_SPLASH";

static lv_obj_t      *s_root          = NULL;
static lv_timer_t    *s_timer         = NULL;
static button_state_t s_a_cache       = BTN_RELEASED;
static lv_obj_t      *s_lbl_press_a   = NULL;
static bool           s_press_a_blink = false;

static void splash_tick(lv_timer_t *t)
{
    (void)t;

    /* Blink simples "Press A" — 500 ms on/off. */
    static int8_t blink_ticks = 0;
    if (++blink_ticks >= 5) {     /* 5 * UI_TICK_MS = 500ms */
        blink_ticks = 0;
        s_press_a_blink = !s_press_a_blink;
        lv_obj_set_style_opa(s_lbl_press_a,
                             s_press_a_blink ? LV_OPA_TRANSP : LV_OPA_COVER,
                             LV_PART_MAIN);
    }

    /* Avanca para o menu ao pressionar A (edge). FSM dirige a troca de tela. */
    if (ui_btn_edge(BTN_A, &s_a_cache)) {
        ESP_LOGI(TAG, "A pressionado na splash -> menu");
        fsm_set_state(GAME_STATE_MENU);
    }
}

void screen_splash_build(void)
{
    /* Chamado pelo router DENTRO de lv_lock. */
    s_root = lv_obj_create(lv_screen_active());
    lv_obj_set_size(s_root, 480, 320);
    lv_obj_set_pos(s_root, 0, 0);
    lv_obj_set_style_bg_color(s_root, UI_COLOR_BG, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(s_root, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(s_root, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(s_root, 0, LV_PART_MAIN);
    lv_obj_remove_flag(s_root, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *title = lv_label_create(s_root);
    lv_label_set_text(title, "CyberSec");
    lv_obj_set_style_text_color(title, UI_COLOR_ACCENT, LV_PART_MAIN);
    lv_obj_align(title, LV_ALIGN_CENTER, 0, -50);

    lv_obj_t *subtitle = lv_label_create(s_root);
    lv_label_set_text(subtitle, "Network Defender");
    lv_obj_set_style_text_color(subtitle, UI_COLOR_TEXT, LV_PART_MAIN);
    lv_obj_align(subtitle, LV_ALIGN_CENTER, 0, -10);

    s_lbl_press_a = lv_label_create(s_root);
    lv_label_set_text(s_lbl_press_a, "Press A");
    lv_obj_set_style_text_color(s_lbl_press_a, UI_COLOR_DIM, LV_PART_MAIN);
    lv_obj_align(s_lbl_press_a, LV_ALIGN_CENTER, 0, 60);

    s_a_cache       = button_hal_peek(BTN_A);
    s_press_a_blink = false;

    s_timer = lv_timer_create(splash_tick, UI_TICK_MS, NULL);
    ESP_LOGI(TAG, "splash built");
}

void screen_splash_destroy(void)
{
    if (s_timer) {
        lv_timer_delete(s_timer);
        s_timer = NULL;
    }
    if (s_root) {
        lv_obj_delete(s_root);
        s_root = NULL;
    }
}
