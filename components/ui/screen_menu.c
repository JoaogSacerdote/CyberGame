#include "ui.h"
#include "ui_internal.h"

#include "esp_log.h"
#include "esp_timer.h"
#include "lvgl.h"
#include "button_hal.h"
#include "joystick_hal.h"
#include "fsm.h"

static const char *TAG = "UI_MENU";

#define MENU_OPTIONS  3
static const char *MENU_LABELS[MENU_OPTIONS] = {
    "Iniciar Expediente",
    "Ranking",
    "Sobre",
};

static lv_obj_t   *s_root            = NULL;
static lv_timer_t *s_timer           = NULL;
static lv_obj_t   *s_lbls[MENU_OPTIONS] = { NULL };
static lv_obj_t   *s_cursor          = NULL;

static int            s_selected     = 0;
static int            s_drawn        = -1;  /* indice efetivamente desenhado */
static button_state_t s_a_cache      = BTN_RELEASED;
static button_state_t s_b_cache      = BTN_RELEASED;
static int64_t        s_last_nav_us  = 0;

static void redraw_cursor_if_needed(void)
{
    if (s_selected == s_drawn) return;
    /* Cursor segue o label selecionado (alinhado a esquerda dele). */
    lv_obj_align_to(s_cursor, s_lbls[s_selected], LV_ALIGN_OUT_LEFT_MID, -10, 0);
    /* Realca o label selecionado, desreaalca os demais. */
    for (int i = 0; i < MENU_OPTIONS; ++i) {
        lv_obj_set_style_text_color(s_lbls[i],
                                    (i == s_selected) ? UI_COLOR_ACCENT : UI_COLOR_TEXT,
                                    LV_PART_MAIN);
    }
    s_drawn = s_selected;
}

static void menu_tick(lv_timer_t *t)
{
    (void)t;

    /* Navegacao discreta com debounce temporal — joystick analogico nao deve
     * varrer 3 itens por meio segundo. */
    const joystick_data_t j = joystick_hal_get_state();
    const int64_t now_us = esp_timer_get_time();
    const bool nav_unlocked = (now_us - s_last_nav_us) >= (UI_MENU_NAV_DEBOUNCE_MS * 1000);

    if (nav_unlocked) {
        int delta = 0;
        if      (j.y >=  UI_JOYSTICK_DEFLEXAO_MIN) delta = -1;   /* y=+ = cima */
        else if (j.y <= -UI_JOYSTICK_DEFLEXAO_MIN) delta = +1;   /* y=- = baixo */
        if (delta != 0) {
            s_selected = (s_selected + delta + MENU_OPTIONS) % MENU_OPTIONS;
            s_last_nav_us = now_us;
        }
    }
    redraw_cursor_if_needed();

    /* Confirma com A. FSM dirige troca de tela. */
    if (ui_btn_edge(BTN_A, &s_a_cache)) {
        ESP_LOGI(TAG, "A em '%s' (idx %d)", MENU_LABELS[s_selected], s_selected);
        switch (s_selected) {
            case 0: fsm_set_state(GAME_STATE_GAMEPLAY); break;
            case 1: ESP_LOGI(TAG, "Ranking — TODO"); break;
            case 2: ESP_LOGI(TAG, "Sobre — TODO"); break;
            default: break;
        }
    }
    /* B na splash do menu volta para a splash inicial. */
    if (ui_btn_edge(BTN_B, &s_b_cache)) {
        fsm_set_state(GAME_STATE_SPLASH);
    }
}

void screen_menu_build(void)
{
    s_root = lv_obj_create(lv_screen_active());
    lv_obj_set_size(s_root, 480, 320);
    lv_obj_set_pos(s_root, 0, 0);
    lv_obj_set_style_bg_color(s_root, UI_COLOR_BG, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(s_root, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(s_root, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(s_root, 0, LV_PART_MAIN);
    lv_obj_remove_flag(s_root, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *title = lv_label_create(s_root);
    lv_label_set_text(title, "MENU PRINCIPAL");
    lv_obj_set_style_text_color(title, UI_COLOR_DIM, LV_PART_MAIN);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 30);

    for (int i = 0; i < MENU_OPTIONS; ++i) {
        s_lbls[i] = lv_label_create(s_root);
        lv_label_set_text(s_lbls[i], MENU_LABELS[i]);
        lv_obj_set_style_text_color(s_lbls[i], UI_COLOR_TEXT, LV_PART_MAIN);
        lv_obj_align(s_lbls[i], LV_ALIGN_CENTER, 0, -30 + i * 30);
    }

    s_cursor = lv_label_create(s_root);
    lv_label_set_text(s_cursor, ">");
    lv_obj_set_style_text_color(s_cursor, UI_COLOR_ACCENT, LV_PART_MAIN);

    s_selected = 0;
    s_drawn    = -1;
    redraw_cursor_if_needed();

    s_a_cache     = button_hal_peek(BTN_A);
    s_b_cache     = button_hal_peek(BTN_B);
    s_last_nav_us = esp_timer_get_time();

    s_timer = lv_timer_create(menu_tick, UI_TICK_MS, NULL);
    ESP_LOGI(TAG, "menu built");
}

void screen_menu_destroy(void)
{
    if (s_timer) {
        lv_timer_delete(s_timer);
        s_timer = NULL;
    }
    if (s_root) {
        lv_obj_delete(s_root);
        s_root   = NULL;
        s_cursor = NULL;
        for (int i = 0; i < MENU_OPTIONS; ++i) s_lbls[i] = NULL;
    }
}
