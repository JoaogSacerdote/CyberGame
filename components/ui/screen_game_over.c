#include "ui.h"
#include "ui_internal.h"

#include "esp_log.h"
#include "lvgl.h"
#include "asset_loader.h"
#include "asset_ids.h"
#include "gamestate.h"

static const char *TAG = "UI_GAMEOVER";

static lv_obj_t      *s_root  = NULL;
static loaded_asset_t s_asset = {0};

static void no_scroll(lv_obj_t *o)
{
    lv_obj_remove_flag(o, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scroll_dir(o, LV_DIR_NONE);
}

void screen_game_over_build(void)
{
    const game_result_t result   = gamestate_get_result();
    const bool          vitoria  = (result == RESULT_VITORIA);
    const uint16_t      asset_id = vitoria ? ASSET_TELA_VITORIA : ASSET_TELA_DERROTA;

    s_root = lv_obj_create(lv_screen_active());
    lv_obj_set_size(s_root, 480, 320);
    lv_obj_set_pos(s_root, 0, 0);
    lv_obj_set_style_bg_color(s_root, lv_color_black(), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(s_root, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(s_root, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(s_root, 0, LV_PART_MAIN);
    no_scroll(s_root);

    /* Fundo: TELA_VITORIA.png ou TELA_DERROTA.png */
    const esp_err_t err = asset_loader_load(ASSET_TYPE_SPRITE, asset_id, &s_asset);
    if (err == ESP_OK) {
        lv_obj_t *bg = lv_image_create(s_root);
        lv_image_set_src(bg, &s_asset.dsc);
        lv_obj_set_pos(bg, 0, 0);
        no_scroll(bg);
    } else {
        ESP_LOGE(TAG, "asset %u indisponivel (%s) — usando fallback de cor",
                 asset_id, esp_err_to_name(err));
        lv_obj_set_style_bg_color(s_root,
            vitoria ? lv_color_hex(0x003010) : lv_color_hex(0x2A0810),
            LV_PART_MAIN);
    }

    /* Título com sombra de fundo para legibilidade sobre qualquer asset */
    lv_obj_t *title = lv_label_create(s_root);
    lv_label_set_text(title, vitoria ? "PROMOVIDO!" : "DEMITIDO");
    lv_obj_set_style_text_color(title,
        vitoria ? lv_color_hex(0x00C853) : lv_color_hex(0xFF4040),
        LV_PART_MAIN);
    lv_obj_set_style_bg_color(title, lv_color_black(), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(title, LV_OPA_60, LV_PART_MAIN);
    lv_obj_set_style_pad_all(title, 6, LV_PART_MAIN);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 8);
    no_scroll(title);

    /* Hint de botões */
    lv_obj_t *hint = lv_label_create(s_root);
    lv_label_set_text(hint,
        vitoria
        ? "[A] Jogar novamente    [B] Menu"
        : "[A] Tentar novamente    [B] Menu");
    lv_obj_set_style_text_color(hint, UI_COLOR_DIM, LV_PART_MAIN);
    lv_obj_set_style_bg_color(hint, lv_color_black(), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(hint, LV_OPA_60, LV_PART_MAIN);
    lv_obj_set_style_pad_all(hint, 6, LV_PART_MAIN);
    lv_obj_align(hint, LV_ALIGN_BOTTOM_MID, 0, -8);
    no_scroll(hint);

    ESP_LOGI(TAG, "game over built: %s", vitoria ? "VITORIA" : "DERROTA");
}

void screen_game_over_destroy(void)
{
    if (s_root) { lv_obj_delete(s_root); s_root = NULL; }
    asset_loader_free(&s_asset);
}
