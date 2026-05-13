#include "ui.h"
#include "ui_internal.h"

#include <stdio.h>
#include "esp_log.h"
#include "lvgl.h"
#include "fsm_gameplay.h"

static const char *TAG = "UI_PLACEHOLDER";

static lv_obj_t            *s_root      = NULL;
static lv_obj_t            *s_lbl_sub   = NULL;
static lv_obj_t            *s_lbl_hint  = NULL;
static lv_timer_t          *s_timer     = NULL;
static gameplay_substate_t  s_last_sub  = GAMEPLAY_SUB_MAX;  /* invalido -> primeira leitura atualiza */

static const char *hint_for(gameplay_substate_t s)
{
    switch (s) {
        case GAMEPLAY_SUB_EXPLORANDO:      return "Y=terminal  B=menu  START=pause";
        case GAMEPLAY_SUB_TERMINAL_ABERTO: return "A=iniciar mitigacao  B=fechar  START=pause";
        case GAMEPLAY_SUB_WAITING_CARD:    return "X=mock leitura NFC  B=voltar  START=pause";
        case GAMEPLAY_SUB_ACTION_LOCK:     return "(travado digitando 1.5s)  START=pause";
        case GAMEPLAY_SUB_SYSTEM_DEPLOY:   return "(deploy 4s)  START=pause";
        default:                           return "";
    }
}

static void placeholder_tick(lv_timer_t *t)
{
    (void)t;
    /* Diff-gating: so toca os labels se o sub-estado mudou. A FSM ja loga
     * a transicao; aqui so refletimos visualmente. */
    const gameplay_substate_t now = fsm_get_gameplay_substate();
    if (now == s_last_sub) return;

    char buf[48];
    snprintf(buf, sizeof(buf), "SUBESTADO: %s", fsm_gameplay_substate_name(now));
    lv_label_set_text(s_lbl_sub, buf);
    lv_label_set_text(s_lbl_hint, hint_for(now));
    s_last_sub = now;
}

void screen_placeholder_build(void)
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
    lv_label_set_text(title, "GAMEPLAY EM CONSTRUCAO");
    lv_obj_set_style_text_color(title, UI_COLOR_WARN, LV_PART_MAIN);
    lv_obj_align(title, LV_ALIGN_CENTER, 0, -60);

    s_lbl_sub = lv_label_create(s_root);
    lv_label_set_text(s_lbl_sub, "SUBESTADO: ?");
    lv_obj_set_style_text_color(s_lbl_sub, UI_COLOR_ACCENT, LV_PART_MAIN);
    lv_obj_align(s_lbl_sub, LV_ALIGN_CENTER, 0, 0);

    s_lbl_hint = lv_label_create(s_root);
    lv_label_set_text(s_lbl_hint, "");
    lv_obj_set_style_text_color(s_lbl_hint, UI_COLOR_DIM, LV_PART_MAIN);
    lv_obj_align(s_lbl_hint, LV_ALIGN_CENTER, 0, 60);

    s_last_sub = GAMEPLAY_SUB_MAX;
    s_timer = lv_timer_create(placeholder_tick, UI_TICK_MS, NULL);
    ESP_LOGI(TAG, "placeholder built");
}

void screen_placeholder_destroy(void)
{
    if (s_timer) { lv_timer_delete(s_timer); s_timer = NULL; }
    if (s_root)  {
        lv_obj_delete(s_root);
        s_root     = NULL;
        s_lbl_sub  = NULL;
        s_lbl_hint = NULL;
    }
}
