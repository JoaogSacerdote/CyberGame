#include "screen_servidor_menu.h"
#include "ui_internal.h"

#include <stdbool.h>
#include "esp_log.h"
#include "lvgl.h"
#include "joystick_hal.h"
#include "button_hal.h"

static const char *TAG = "SRV_MENU";

#define N_OPTIONS  2

static const char *OPTION_LABELS[N_OPTIONS] = {
    "Servidor de Backup",
    "Servidor WEB",
};

#define BG_COLOR     lv_color_hex(0x1A3A4A)
#define TITLE_COLOR  lv_color_hex(0x80D0FF)
#define TEXT_SEL     lv_color_hex(0xFFFFFF)
#define TEXT_UNSEL   lv_color_hex(0x607080)

static lv_obj_t          *s_overlay       = NULL;
static lv_obj_t          *s_lbl[N_OPTIONS] = {NULL};
static lv_timer_t        *s_timer         = NULL;
static servidor_menu_cb_t s_done_cb       = NULL;
static uint8_t            s_sel           = 0;
static uint32_t           s_nav_cooldown  = 0;

static button_state_t s_a_cache = BTN_RELEASED;
static button_state_t s_b_cache = BTN_RELEASED;

static void render_options(void)
{
    char buf[32];
    for (int i = 0; i < N_OPTIONS; i++) {
        if (i == s_sel) {
            snprintf(buf, sizeof(buf), "> %s", OPTION_LABELS[i]);
            lv_label_set_text(s_lbl[i], buf);
            lv_obj_set_style_text_color(s_lbl[i], TEXT_SEL, LV_PART_MAIN);
        } else {
            lv_label_set_text(s_lbl[i], OPTION_LABELS[i]);
            lv_obj_set_style_text_color(s_lbl[i], TEXT_UNSEL, LV_PART_MAIN);
        }
    }
}

static void srv_menu_tick(lv_timer_t *t)
{
    (void)t;
    if (!s_overlay) return;

    /* Navegacao por joystick com cooldown */
    if (s_nav_cooldown > UI_TICK_MS) {
        s_nav_cooldown -= UI_TICK_MS;
    } else {
        s_nav_cooldown = 0;
    }

    if (s_nav_cooldown == 0) {
        const joystick_data_t j = joystick_hal_get_state();
        if (j.y > UI_JOYSTICK_DEFLEXAO_MIN) {
            s_sel = (s_sel + 1) % N_OPTIONS;
            render_options();
            s_nav_cooldown = UI_MENU_NAV_DEBOUNCE_MS;
        } else if (j.y < -UI_JOYSTICK_DEFLEXAO_MIN) {
            s_sel = (uint8_t)((s_sel + N_OPTIONS - 1) % N_OPTIONS);
            render_options();
            s_nav_cooldown = UI_MENU_NAV_DEBOUNCE_MS;
        }
    }

    if (ui_btn_edge(BTN_A, &s_a_cache)) {
        ESP_LOGI(TAG, "opcao selecionada: %u (%s)", s_sel, OPTION_LABELS[s_sel]);
        const servidor_menu_result_t result =
            (s_sel == 0) ? SERVIDOR_MENU_BACKUP : SERVIDOR_MENU_WEB;
        servidor_menu_cb_t cb = s_done_cb;
        screen_servidor_menu_destroy();
        if (cb) cb(result);
        return;
    }

    if (ui_btn_edge(BTN_B, &s_b_cache)) {
        servidor_menu_cb_t cb = s_done_cb;
        screen_servidor_menu_destroy();
        if (cb) cb(SERVIDOR_MENU_CANCELADO);
        return;
    }
}

void screen_servidor_menu_build(servidor_menu_cb_t done_cb)
{
    if (s_overlay) return;

    s_done_cb      = done_cb;
    s_sel          = 0;
    s_nav_cooldown = 0;
    s_a_cache      = button_hal_peek(BTN_A);
    s_b_cache      = button_hal_peek(BTN_B);

    s_overlay = lv_obj_create(lv_screen_active());
    lv_obj_set_size(s_overlay, 480, 320);
    lv_obj_set_pos(s_overlay, 0, 0);
    lv_obj_set_style_bg_color(s_overlay, BG_COLOR, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(s_overlay, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(s_overlay, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(s_overlay, 0, LV_PART_MAIN);
    lv_obj_remove_flag(s_overlay, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *title = lv_label_create(s_overlay);
    lv_label_set_text(title, "[ SERVIDOR ]");
    lv_obj_set_style_text_color(title, TITLE_COLOR, LV_PART_MAIN);
    lv_obj_set_pos(title, 180, 90);

    for (int i = 0; i < N_OPTIONS; i++) {
        s_lbl[i] = lv_label_create(s_overlay);
        lv_obj_set_pos(s_lbl[i], 160, 148 + i * 40);
    }
    render_options();

    lv_obj_t *hint = lv_label_create(s_overlay);
    lv_label_set_text(hint, "[A] Confirmar   [B] Voltar");
    lv_obj_set_style_text_color(hint, TEXT_UNSEL, LV_PART_MAIN);
    lv_obj_set_pos(hint, 145, 265);

    s_timer = lv_timer_create(srv_menu_tick, UI_TICK_MS, NULL);
    ESP_LOGI(TAG, "servidor menu aberto");
}

void screen_servidor_menu_destroy(void)
{
    if (s_timer) { lv_timer_delete(s_timer); s_timer = NULL; }
    s_done_cb = NULL;
    for (int i = 0; i < N_OPTIONS; i++) s_lbl[i] = NULL;
    if (s_overlay) {
        lv_obj_delete(s_overlay);
        s_overlay = NULL;
    }
}

bool screen_servidor_menu_is_open(void) { return s_overlay != NULL; }
