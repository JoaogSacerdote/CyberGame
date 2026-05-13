#include "ui.h"
#include "ui_internal.h"

#include "esp_log.h"
#include "lvgl.h"

static const char *TAG = "UI";

static ui_screen_t s_active = UI_SCREEN_NONE;
static bool        s_inited = false;

static void destroy_active(void)
{
    switch (s_active) {
        case UI_SCREEN_SPLASH:      screen_splash_destroy(); break;
        case UI_SCREEN_MENU:        screen_menu_destroy(); break;
        case UI_SCREEN_PLACEHOLDER: screen_placeholder_destroy(); break;
        case UI_SCREEN_PAUSE:       screen_pause_destroy(); break;
        default: break;
    }
    s_active = UI_SCREEN_NONE;
}

static void show_screen(ui_screen_t target, void (*build_fn)(void))
{
    if (s_active == target) {
        return;
    }
    lv_lock();
    destroy_active();
    build_fn();
    s_active = target;
    lv_unlock();
    ESP_LOGI(TAG, "ui_show -> %d", (int)target);
}

esp_err_t ui_init(void)
{
    if (s_inited) {
        return ESP_OK;
    }
    lv_lock();
    lv_obj_t *scr = lv_screen_active();
    lv_obj_set_style_bg_color(scr, UI_COLOR_BG, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_pad_all(scr, 0, LV_PART_MAIN);
    lv_obj_remove_flag(scr, LV_OBJ_FLAG_SCROLLABLE);
    lv_unlock();
    s_inited = true;
    ESP_LOGI(TAG, "ui_init OK");
    return ESP_OK;
}

void ui_show_splash(void)      { show_screen(UI_SCREEN_SPLASH,      screen_splash_build); }
void ui_show_menu(void)        { show_screen(UI_SCREEN_MENU,        screen_menu_build); }
void ui_show_placeholder(void) { show_screen(UI_SCREEN_PLACEHOLDER, screen_placeholder_build); }
void ui_show_pause(void)       { show_screen(UI_SCREEN_PAUSE,       screen_pause_build); }

ui_screen_t ui_get_active(void) { return s_active; }
