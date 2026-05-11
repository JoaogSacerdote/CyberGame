#include "ui_debug.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"

#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_timer.h"

#include "lvgl.h"

#include "button_hal.h"
#include "joystick_hal.h"
#include "hal_bridge.h"

static const char *TAG = "UI_DEBUG";

#define UI_REFRESH_PERIOD_MS    100

#define COLOR_BG                lv_color_hex(0x101418)
#define COLOR_PANEL             lv_color_hex(0x1B2128)
#define COLOR_HEADER            lv_color_hex(0x0A2540)
#define COLOR_TEXT              lv_color_hex(0xE6E6E6)
#define COLOR_TEXT_DIM          lv_color_hex(0x808890)
#define COLOR_BTN_OFF           lv_color_hex(0x3A4148)
#define COLOR_BTN_ON            lv_color_hex(0x00C853)
#define COLOR_JOY_BG            lv_color_hex(0x0A0D10)
#define COLOR_JOY_DOT           lv_color_hex(0xFFC107)
#define COLOR_NFC_OFF           lv_color_hex(0x3A4148)
#define COLOR_NFC_ON            lv_color_hex(0xFFC107)

#define JOY_BOX_SIZE            100
#define JOY_DOT_SIZE            10

static SemaphoreHandle_t s_lock = NULL;

/* Ultimo estado reportado pelo consumer do NFC. Protegido por s_lock. */
static nfc_card_t s_last_card = { 0 };
static bool       s_has_card  = false;
static bool       s_scanning  = false;

/* Widgets — todos criados em ui_debug_init() sob lv_lock(). */
static lv_obj_t *s_lbl_uptime      = NULL;
static lv_obj_t *s_lbl_heap        = NULL;

static lv_obj_t *s_btn_dots[BTN_MAX_COUNT]   = { NULL };
static lv_obj_t *s_btn_labels[BTN_MAX_COUNT] = { NULL };

static lv_obj_t *s_lbl_joy_xy      = NULL;
static lv_obj_t *s_joy_box         = NULL;
static lv_obj_t *s_joy_dot         = NULL;

static lv_obj_t *s_lbl_nfc_state   = NULL;
static lv_obj_t *s_lbl_nfc_uid     = NULL;

static int64_t s_boot_us = 0;

/* "Cache" do que ja foi desenhado para evitar invalidar areas LVGL a cada
 * refresh quando nada mudou. lv_obj_set_style_bg_color marca a area como
 * dirty mesmo se o valor for o mesmo — sem esse cache, 4 botoes × 10 Hz
 * = 40 invalidacoes/s constantes, que saturam o LVGL no CPU 0 e disparam
 * o task_wdt. */
static bool     s_btn_drawn[BTN_MAX_COUNT] = { false, false, false, false };
static bool     s_nfc_drawn_scanning       = false;
static bool     s_nfc_drawn_has_card       = false;
static uint8_t  s_nfc_drawn_uid[NFC_UID_MAX_LEN] = { 0 };
static uint8_t  s_nfc_drawn_uid_len        = 0;
static bool     s_nfc_first_paint          = true;

/* Deadzone visual do joystick: ruido analogico faz X/Y oscilar +/- 1-2 unidades
 * mesmo "parado". Sem isso, lv_label_set_text_fmt reformata e invalida a cada
 * refresh, saturando o LVGL e disparando task_wdt. */
#define JOY_VISUAL_DELTA   3
static int      s_joy_last_x               = 0;
static int      s_joy_last_y               = 0;
static bool     s_joy_first_paint          = true;

/* Header so re-formata se uptime mudou de segundo ou se heap mudou de KB. */
static int      s_last_uptime_s            = -1;
static unsigned s_last_dram_kb             = (unsigned)-1;
static unsigned s_last_psram_kb            = (unsigned)-1;

/* ============================================================ helpers === */

static void style_panel(lv_obj_t *obj)
{
    lv_obj_set_style_bg_color(obj, COLOR_PANEL, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(obj, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(obj, 4, LV_PART_MAIN);
    lv_obj_set_style_pad_all(obj, 6, LV_PART_MAIN);
}

static lv_obj_t *make_label(lv_obj_t *parent, const char *text, lv_color_t color)
{
    lv_obj_t *lbl = lv_label_create(parent);
    lv_label_set_text(lbl, text);
    lv_obj_set_style_text_color(lbl, color, LV_PART_MAIN);
    return lbl;
}

/* ====================================================== build screen === */

static void build_screen(void)
{
    lv_obj_t *scr = lv_screen_active();
    lv_obj_set_style_bg_color(scr, COLOR_BG, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_pad_all(scr, 0, LV_PART_MAIN);
    lv_obj_remove_flag(scr, LV_OBJ_FLAG_SCROLLABLE);

    /* ---- Header (full width, 36 px) ---- */
    lv_obj_t *hdr = lv_obj_create(scr);
    lv_obj_set_size(hdr, 480, 36);
    lv_obj_set_pos(hdr, 0, 0);
    lv_obj_set_style_bg_color(hdr, COLOR_HEADER, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(hdr, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(hdr, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(hdr, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(hdr, 8, LV_PART_MAIN);
    lv_obj_remove_flag(hdr, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *title = make_label(hdr, "CyberGame - DEBUG HALs", COLOR_TEXT);
    lv_obj_align(title, LV_ALIGN_LEFT_MID, 0, 0);

    s_lbl_uptime = make_label(hdr, "up: 0 s", COLOR_TEXT_DIM);
    lv_obj_align(s_lbl_uptime, LV_ALIGN_RIGHT_MID, -150, 0);

    s_lbl_heap = make_label(hdr, "heap: -- KB", COLOR_TEXT_DIM);
    lv_obj_align(s_lbl_heap, LV_ALIGN_RIGHT_MID, 0, 0);

    /* ---- Painel BOTOES (esquerda, abaixo do header) ---- */
    lv_obj_t *btn_panel = lv_obj_create(scr);
    lv_obj_set_size(btn_panel, 220, 140);
    lv_obj_set_pos(btn_panel, 10, 46);
    style_panel(btn_panel);
    lv_obj_remove_flag(btn_panel, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *btn_title = make_label(btn_panel, "BOTOES", COLOR_TEXT_DIM);
    lv_obj_align(btn_title, LV_ALIGN_TOP_LEFT, 0, 0);

    static const char *names[BTN_MAX_COUNT] = { "A", "B", "X", "Y" };
    for (int i = 0; i < BTN_MAX_COUNT; ++i) {
        const int col_x = 10 + i * 50;
        const int row_y = 35;

        s_btn_dots[i] = lv_obj_create(btn_panel);
        lv_obj_set_size(s_btn_dots[i], 36, 36);
        lv_obj_set_pos(s_btn_dots[i], col_x, row_y);
        lv_obj_set_style_radius(s_btn_dots[i], LV_RADIUS_CIRCLE, LV_PART_MAIN);
        lv_obj_set_style_bg_color(s_btn_dots[i], COLOR_BTN_OFF, LV_PART_MAIN);
        lv_obj_set_style_bg_opa(s_btn_dots[i], LV_OPA_COVER, LV_PART_MAIN);
        lv_obj_set_style_border_width(s_btn_dots[i], 0, LV_PART_MAIN);
        lv_obj_remove_flag(s_btn_dots[i], LV_OBJ_FLAG_SCROLLABLE);

        s_btn_labels[i] = make_label(s_btn_dots[i], names[i], COLOR_TEXT);
        lv_obj_center(s_btn_labels[i]);
    }

    /* ---- Painel JOYSTICK (direita, abaixo do header) ---- */
    lv_obj_t *joy_panel = lv_obj_create(scr);
    lv_obj_set_size(joy_panel, 230, 160);
    lv_obj_set_pos(joy_panel, 240, 46);
    style_panel(joy_panel);
    lv_obj_remove_flag(joy_panel, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *joy_title = make_label(joy_panel, "JOYSTICK", COLOR_TEXT_DIM);
    lv_obj_align(joy_title, LV_ALIGN_TOP_LEFT, 0, 0);

    s_lbl_joy_xy = make_label(joy_panel, "X=   0  Y=   0", COLOR_TEXT);
    lv_obj_align(s_lbl_joy_xy, LV_ALIGN_TOP_LEFT, 0, 22);

    s_joy_box = lv_obj_create(joy_panel);
    lv_obj_set_size(s_joy_box, JOY_BOX_SIZE, JOY_BOX_SIZE);
    lv_obj_align(s_joy_box, LV_ALIGN_BOTTOM_RIGHT, 0, 0);
    lv_obj_set_style_bg_color(s_joy_box, COLOR_JOY_BG, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(s_joy_box, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(s_joy_box, 1, LV_PART_MAIN);
    lv_obj_set_style_border_color(s_joy_box, COLOR_TEXT_DIM, LV_PART_MAIN);
    lv_obj_set_style_radius(s_joy_box, 2, LV_PART_MAIN);
    lv_obj_set_style_pad_all(s_joy_box, 0, LV_PART_MAIN);
    lv_obj_remove_flag(s_joy_box, LV_OBJ_FLAG_SCROLLABLE);

    s_joy_dot = lv_obj_create(s_joy_box);
    lv_obj_set_size(s_joy_dot, JOY_DOT_SIZE, JOY_DOT_SIZE);
    lv_obj_set_style_radius(s_joy_dot, LV_RADIUS_CIRCLE, LV_PART_MAIN);
    lv_obj_set_style_bg_color(s_joy_dot, COLOR_JOY_DOT, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(s_joy_dot, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(s_joy_dot, 0, LV_PART_MAIN);
    lv_obj_remove_flag(s_joy_dot, LV_OBJ_FLAG_SCROLLABLE);
    /* Centro inicial. */
    lv_obj_set_pos(s_joy_dot,
                   (JOY_BOX_SIZE - JOY_DOT_SIZE) / 2,
                   (JOY_BOX_SIZE - JOY_DOT_SIZE) / 2);

    /* ---- Painel NFC (largura completa, parte de baixo) ---- */
    lv_obj_t *nfc_panel = lv_obj_create(scr);
    lv_obj_set_size(nfc_panel, 460, 100);
    lv_obj_set_pos(nfc_panel, 10, 212);
    style_panel(nfc_panel);
    lv_obj_remove_flag(nfc_panel, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *nfc_title = make_label(nfc_panel, "NFC", COLOR_TEXT_DIM);
    lv_obj_align(nfc_title, LV_ALIGN_TOP_LEFT, 0, 0);

    s_lbl_nfc_state = make_label(nfc_panel, "SCAN OFF", COLOR_NFC_OFF);
    lv_obj_align(s_lbl_nfc_state, LV_ALIGN_TOP_LEFT, 0, 26);

    s_lbl_nfc_uid = make_label(nfc_panel, "UID: --", COLOR_TEXT);
    lv_obj_align(s_lbl_nfc_uid, LV_ALIGN_TOP_LEFT, 0, 54);
}

/* ====================================================== refresh task === */

static void refresh_buttons(void)
{
    for (int i = 0; i < BTN_MAX_COUNT; ++i) {
        const bool pressed = (button_hal_peek((button_id_t)i) == BTN_PRESSED);
        if (pressed == s_btn_drawn[i]) {
            continue;
        }
        lv_obj_set_style_bg_color(s_btn_dots[i],
                                  pressed ? COLOR_BTN_ON : COLOR_BTN_OFF,
                                  LV_PART_MAIN);
        s_btn_drawn[i] = pressed;
    }
}

static void refresh_joystick(void)
{
    const joystick_data_t j = joystick_hal_get_state();
    const int nx = (int)j.x;
    const int ny = (int)j.y;

    if (!s_joy_first_paint &&
        abs(nx - s_joy_last_x) < JOY_VISUAL_DELTA &&
        abs(ny - s_joy_last_y) < JOY_VISUAL_DELTA) {
        return;
    }

    lv_label_set_text_fmt(s_lbl_joy_xy, "X=%4d  Y=%4d", nx, ny);

    /* Mapeia -100..+100 para o range da caixa (com folga do tamanho do dot).
     * Eixo Y: o joystick reporta +Y = "cima", LVGL trata Y crescente para baixo,
     * entao invertemos. */
    const int range = JOY_BOX_SIZE - JOY_DOT_SIZE;
    const int cx    = (nx + 100) * range / 200;
    const int cy    = (100 - ny) * range / 200;
    lv_obj_set_pos(s_joy_dot, cx, cy);

    s_joy_last_x      = nx;
    s_joy_last_y      = ny;
    s_joy_first_paint = false;
}

static void refresh_header(void)
{
    const int64_t now_us = esp_timer_get_time();
    const int seconds = (int)((now_us - s_boot_us) / 1000000);
    if (seconds != s_last_uptime_s) {
        lv_label_set_text_fmt(s_lbl_uptime, "up: %d s", seconds);
        s_last_uptime_s = seconds;
    }

    const unsigned dram_kb  = (unsigned)(heap_caps_get_free_size(MALLOC_CAP_INTERNAL) / 1024);
    const unsigned psram_kb = (unsigned)(heap_caps_get_free_size(MALLOC_CAP_SPIRAM)   / 1024);
    if (dram_kb != s_last_dram_kb || psram_kb != s_last_psram_kb) {
        lv_label_set_text_fmt(s_lbl_heap, "heap: %u/%u KB", dram_kb, psram_kb);
        s_last_dram_kb  = dram_kb;
        s_last_psram_kb = psram_kb;
    }
}

static void refresh_nfc(void)
{
    bool scanning;
    bool has_card;
    nfc_card_t card;

    xSemaphoreTake(s_lock, portMAX_DELAY);
    scanning = s_scanning;
    has_card = s_has_card;
    card     = s_last_card;
    xSemaphoreGive(s_lock);

    /* Estado SCAN ON/OFF — so atualiza quando muda (ou no primeiro paint). */
    if (s_nfc_first_paint || scanning != s_nfc_drawn_scanning) {
        lv_label_set_text(s_lbl_nfc_state, scanning ? "SCAN ON" : "SCAN OFF");
        lv_obj_set_style_text_color(s_lbl_nfc_state,
                                    scanning ? COLOR_NFC_ON : COLOR_NFC_OFF,
                                    LV_PART_MAIN);
        s_nfc_drawn_scanning = scanning;
    }

    /* UID — so atualiza quando muda. */
    const bool uid_changed =
        (has_card != s_nfc_drawn_has_card) ||
        (has_card && (card.uid_len != s_nfc_drawn_uid_len ||
                      memcmp(card.uid, s_nfc_drawn_uid, card.uid_len) != 0));

    if (s_nfc_first_paint || uid_changed) {
        if (!has_card) {
            lv_label_set_text(s_lbl_nfc_uid, "UID: --");
        } else {
            char buf[8 + NFC_UID_MAX_LEN * 2 + 1];
            int off = snprintf(buf, sizeof(buf), "UID: ");
            for (int i = 0; i < card.uid_len && i < NFC_UID_MAX_LEN; ++i) {
                off += snprintf(buf + off, sizeof(buf) - off, "%02X", card.uid[i]);
            }
            lv_label_set_text(s_lbl_nfc_uid, buf);
        }
        s_nfc_drawn_has_card = has_card;
        if (has_card) {
            memcpy(s_nfc_drawn_uid, card.uid, card.uid_len);
            s_nfc_drawn_uid_len = card.uid_len;
        }
    }
    s_nfc_first_paint = false;
}

static void refresh_task(void *pv)
{
    (void)pv;
    while (1) {
        lv_lock();
        refresh_buttons();
        refresh_joystick();
        refresh_header();
        refresh_nfc();
        lv_unlock();
        vTaskDelay(pdMS_TO_TICKS(UI_REFRESH_PERIOD_MS));
    }
}

/* ============================================================== API === */

esp_err_t ui_debug_init(void)
{
    if (s_lock) {
        return ESP_OK;
    }
    s_lock = xSemaphoreCreateMutex();
    if (!s_lock) {
        ESP_LOGE(TAG, "mutex alloc falhou");
        return ESP_ERR_NO_MEM;
    }

    s_boot_us = esp_timer_get_time();

    lv_lock();
    build_screen();
    lv_unlock();

    if (xTaskCreate(refresh_task, "ui_dbg", 4096, NULL, 4, NULL) != pdPASS) {
        ESP_LOGE(TAG, "xTaskCreate(ui_dbg) falhou");
        return ESP_ERR_NO_MEM;
    }
    ESP_LOGI(TAG, "ui_debug iniciado (refresh %d ms)", UI_REFRESH_PERIOD_MS);
    return ESP_OK;
}

void ui_debug_set_nfc_card(const nfc_card_t *card)
{
    if (!card || !s_lock) return;
    xSemaphoreTake(s_lock, portMAX_DELAY);
    s_last_card = *card;
    s_has_card  = true;
    xSemaphoreGive(s_lock);
}

void ui_debug_set_nfc_scanning(bool on)
{
    if (!s_lock) return;
    xSemaphoreTake(s_lock, portMAX_DELAY);
    s_scanning = on;
    xSemaphoreGive(s_lock);
}
