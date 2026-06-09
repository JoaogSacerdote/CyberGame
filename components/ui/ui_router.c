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
        case UI_SCREEN_RECEPCAO:    screen_recepcao_destroy(); break;
        case UI_SCREEN_EMPRESA:     screen_empresa_destroy(); break;
        case UI_SCREEN_GAME_OVER:   screen_game_over_destroy(); break;
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
    /* Forca redraw da tela inteira: sem isso, o partial render so flusha
     * as areas dos widgets recem-criados e o resto fica com lixo do GRAM
     * do controlador (sintoma: "ruido sobreposto ao texto"). */
    lv_obj_invalidate(lv_screen_active());
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
    /* Zera tambem o scroll_dir: remove_flag(SCROLLABLE) so desabilita scroll
     * por toque — o layout_update_core do refresh ainda chama
     * lv_obj_readjust_scroll se scroll_dir != NONE (default e LV_DIR_ALL).
     * Sem isso, o readjust itera filhos e crasha (LoadProhibited). */
    lv_obj_set_scroll_dir(scr, LV_DIR_NONE);
    lv_unlock();
    s_inited = true;
    ESP_LOGI(TAG, "ui_init OK");
    return ESP_OK;
}

void ui_show_splash(void)      { show_screen(UI_SCREEN_SPLASH,      screen_splash_build); }
void ui_show_menu(void)        { show_screen(UI_SCREEN_MENU,        screen_menu_build); }
void ui_show_placeholder(void) { show_screen(UI_SCREEN_PLACEHOLDER, screen_placeholder_build); }
void ui_show_pause(void)       { show_screen(UI_SCREEN_PAUSE,       screen_pause_build); }
void ui_show_recepcao(void)    { show_screen(UI_SCREEN_RECEPCAO,    screen_recepcao_build); }
void ui_show_empresa(void)     { show_screen(UI_SCREEN_EMPRESA,     screen_empresa_build); }
void ui_show_game_over(void)   { show_screen(UI_SCREEN_GAME_OVER,   screen_game_over_build); }

ui_screen_t ui_get_active(void) { return s_active; }
