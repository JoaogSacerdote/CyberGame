#include "screen_tarefa_vermelha.h"
#include "ui_internal.h"

#include <stdint.h>
#include <stdbool.h>
#include "esp_log.h"
#include "lvgl.h"
#include "button_hal.h"
#include "fsm.h"

static const char *TAG = "TAREFA_VM";

#define BG_COLOR    lv_color_hex(0x100808)
#define TITLE_COLOR lv_color_hex(0xFF4040)
#define WARN_COLOR  lv_color_hex(0xFF8080)
#define OK_COLOR    lv_color_hex(0x40FF80)
#define TEXT_COLOR  lv_color_hex(0xE6E6E6)
#define DIM_COLOR   lv_color_hex(0x606060)

#define CLOSE_DELAY_MS  2000u

static lv_obj_t   *s_overlay    = NULL;
static lv_obj_t   *s_lbl_status = NULL;
static lv_obj_t   *s_lbl_info   = NULL;
static lv_timer_t *s_timer      = NULL;

static bool     s_had_attack  = false;
static bool     s_mitigated   = false;
static uint32_t s_close_ms    = 0;

static button_state_t s_b_cache = BTN_RELEASED;

static void vermelha_tick(lv_timer_t *t)
{
    (void)t;
    if (!s_overlay) return;

    if (s_mitigated) {
        s_close_ms += UI_TICK_MS;
        if (s_close_ms >= CLOSE_DELAY_MS) {
            screen_tarefa_vermelha_destroy();
        }
        return;
    }

    const bool attack_now = fsm_get_attack_active();

    /* Ataque que estava ativo quando abrimos desapareceu: foi mitigado */
    if (s_had_attack && !attack_now) {
        s_mitigated = true;
        s_close_ms  = 0;
        lv_label_set_text(s_lbl_status, "ATAQUE MITIGADO!");
        lv_obj_set_style_text_color(s_lbl_status, OK_COLOR, LV_PART_MAIN);
        lv_label_set_text(s_lbl_info, "Sistema protegido.\nFechando...");
        lv_obj_set_style_text_color(s_lbl_info, OK_COLOR, LV_PART_MAIN);
        ESP_LOGI(TAG, "DDoS mitigado — fechando em %ums", CLOSE_DELAY_MS);
        return;
    }

    /* Ataque comecou depois que abrimos (estava normal, agora ha ataque) */
    if (!s_had_attack && attack_now) {
        s_had_attack = true;
        lv_label_set_text(s_lbl_status, "ATAQUE DDoS DETECTADO!");
        lv_obj_set_style_text_color(s_lbl_status, WARN_COLOR, LV_PART_MAIN);
        lv_label_set_text(s_lbl_info,
                          "Sistema sob ataque de negacao de servico.\n"
                          "Apresente a carta:\n"
                          "BALANCEAMENTO DE REDE");
        lv_obj_set_style_text_color(s_lbl_info, TEXT_COLOR, LV_PART_MAIN);
    }

    if (ui_btn_edge(BTN_B, &s_b_cache)) {
        screen_tarefa_vermelha_destroy();
        return;
    }
}

void screen_tarefa_vermelha_build(void)
{
    if (s_overlay) return;

    s_had_attack = fsm_get_attack_active();
    s_mitigated  = false;
    s_close_ms   = 0;
    s_b_cache    = button_hal_peek(BTN_B);

    s_overlay = lv_obj_create(lv_screen_active());
    lv_obj_set_size(s_overlay, 480, 320);
    lv_obj_set_pos(s_overlay, 0, 0);
    lv_obj_set_style_bg_color(s_overlay, BG_COLOR, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(s_overlay, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(s_overlay, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(s_overlay, 0, LV_PART_MAIN);
    lv_obj_remove_flag(s_overlay, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *title = lv_label_create(s_overlay);
    lv_label_set_text(title, "SERVIDOR WEB");
    lv_obj_set_style_text_color(title, TITLE_COLOR, LV_PART_MAIN);
    lv_obj_set_pos(title, 190, 50);

    s_lbl_status = lv_label_create(s_overlay);
    lv_obj_set_pos(s_lbl_status, 80, 110);
    lv_obj_set_width(s_lbl_status, 320);
    lv_label_set_long_mode(s_lbl_status, LV_LABEL_LONG_WRAP);

    s_lbl_info = lv_label_create(s_overlay);
    lv_obj_set_pos(s_lbl_info, 80, 155);
    lv_obj_set_width(s_lbl_info, 320);
    lv_label_set_long_mode(s_lbl_info, LV_LABEL_LONG_WRAP);

    if (s_had_attack) {
        lv_label_set_text(s_lbl_status, "ATAQUE DDoS DETECTADO!");
        lv_obj_set_style_text_color(s_lbl_status, WARN_COLOR, LV_PART_MAIN);
        lv_label_set_text(s_lbl_info,
                          "Sistema sob ataque de negacao de servico.\n"
                          "Apresente a carta:\n"
                          "BALANCEAMENTO DE REDE");
        lv_obj_set_style_text_color(s_lbl_info, TEXT_COLOR, LV_PART_MAIN);
    } else {
        lv_label_set_text(s_lbl_status, "Status: NORMAL");
        lv_obj_set_style_text_color(s_lbl_status, OK_COLOR, LV_PART_MAIN);
        lv_label_set_text(s_lbl_info,
                          "Servidor WEB operacional.\n"
                          "Nenhum ataque detectado no momento.");
        lv_obj_set_style_text_color(s_lbl_info, TEXT_COLOR, LV_PART_MAIN);
    }

    lv_obj_t *hint = lv_label_create(s_overlay);
    lv_label_set_text(hint, "[B] Fechar");
    lv_obj_set_style_text_color(hint, DIM_COLOR, LV_PART_MAIN);
    lv_obj_set_pos(hint, 205, 285);

    s_timer = lv_timer_create(vermelha_tick, UI_TICK_MS, NULL);
    ESP_LOGI(TAG, "servidor WEB aberto (ataque_ativo=%d)", (int)s_had_attack);
}

void screen_tarefa_vermelha_destroy(void)
{
    if (s_timer) { lv_timer_delete(s_timer); s_timer = NULL; }
    s_lbl_status = NULL;
    s_lbl_info   = NULL;
    if (s_overlay) {
        lv_obj_delete(s_overlay);
        s_overlay = NULL;
    }
}

bool screen_tarefa_vermelha_is_open(void) { return s_overlay != NULL; }
