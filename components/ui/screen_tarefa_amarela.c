#include "screen_tarefa_amarela.h"
#include "ui_internal.h"

#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include "esp_log.h"
#include "esp_random.h"
#include "lvgl.h"
#include "asset_loader.h"
#include "asset_ids.h"
#include "button_hal.h"
#include "joystick_hal.h"
#include "gamestate.h"

static const char *TAG = "TAREFA_AM";

/* ── Constantes ──────────────────────────────────────────────────────────── */

#define N_SERVER  6   /* slots no rack de servidores (painel superior) */
#define N_STOCK   4   /* slots de estoque (painel inferior) */

/* Posições dos slots (INTERACOES.txt — pivot bottom-center → draw top-left)
 * draw_x = bc_x - SLOT_W/2    draw_y = bc_y - SLOT_H
 * Servidor: bc_y=127  → draw_y=45   | bc_x={82,146,...} → draw_x={50,114,...}
 * Estoque : bc_y=277  → draw_y=195  | bc_x={104,192,...} → draw_x={72,160,...} */
static const int16_t SRV_X[N_SERVER] = { 50, 114, 178, 242, 306, 370 };
static const int16_t SRV_Y           = 45;
static const int16_t STK_X[N_STOCK]  = { 72, 160, 250, 347 };
static const int16_t STK_Y           = 195;
#define SLOT_W  64
#define SLOT_H  82

/* ── Estado (persistente durante a partida) ─────────────────────────────── */

typedef enum { HD_BOM = 0, HD_RUIM } hd_state_t;

static hd_state_t s_srv[N_SERVER];   /* estado de cada slot do servidor */
static hd_state_t s_stk[N_STOCK];    /* estado de cada slot do estoque  */
static bool s_state_initialized = false;

/* ── Variáveis da sessão de overlay ─────────────────────────────────────── */

typedef enum {
    PANEL_SERVER = 0,
    PANEL_STOCK,
} panel_t;

static panel_t  s_panel  = PANEL_STOCK;   /* panel selecionado pelo cursor */
static uint8_t  s_col    = 0;             /* coluna dentro do panel */
static bool     s_holding = false;        /* jogador segura um HD */
static panel_t  s_hold_panel;             /* de onde o HD foi tirado */
static uint8_t  s_hold_col;               /* coluna de origem */

static lv_obj_t   *s_overlay  = NULL;
static lv_obj_t   *s_slot_srv[N_SERVER] = {NULL};
static lv_obj_t   *s_slot_stk[N_STOCK]  = {NULL};
static lv_obj_t   *s_lbl_status = NULL;
static lv_obj_t   *s_lbl_hold   = NULL;
static lv_timer_t *s_timer      = NULL;

static tarefa_am_cb_t s_done_cb       = NULL;
static bool           s_modo_visualizar = false;  /* true: apenas leitura (ja concluida) */

typedef enum { A_FUNDO = 0, A_HD_BOM, A_HD_RUIM, A_COUNT } slot_t;
static loaded_asset_t s_assets[A_COUNT];

static button_state_t s_a_cache = BTN_RELEASED;
static button_state_t s_b_cache = BTN_RELEASED;

/* ── Helpers ─────────────────────────────────────────────────────────────── */

static void no_scroll(lv_obj_t *o)
{
    lv_obj_remove_flag(o, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scroll_dir(o, LV_DIR_NONE);
}

static hd_state_t *slot_state(panel_t panel, uint8_t col)
{
    return (panel == PANEL_SERVER) ? &s_srv[col] : &s_stk[col];
}

static uint8_t panel_max(panel_t p) { return (p == PANEL_SERVER) ? N_SERVER : N_STOCK; }

static lv_obj_t *slot_widget(panel_t panel, uint8_t col)
{
    return (panel == PANEL_SERVER) ? s_slot_srv[col] : s_slot_stk[col];
}

static void refresh_slot(panel_t panel, uint8_t col)
{
    lv_obj_t *w = slot_widget(panel, col);
    if (!w) return;

    const bool hovered = (s_panel == panel && s_col == col);
    const hd_state_t st = *slot_state(panel, col);

    /* Borda: amarela se cursor, verde se cursor no panel do holding */
    lv_color_t border_col;
    if (hovered && s_holding && s_hold_panel == panel && s_hold_col == col)
        border_col = lv_color_hex(0xFF8800);          /* laranja — origem do carry */
    else if (hovered)
        border_col = lv_color_hex(0xFFD000);          /* amarelo — cursor normal */
    else
        border_col = lv_color_hex(0x444444);           /* cinza */

    lv_obj_set_style_border_color(w, border_col, LV_PART_MAIN);
    lv_obj_set_style_border_width(w, hovered ? 3 : 1, LV_PART_MAIN);

    /* Fundo: vazio se este slot é a origem do carry */
    bool is_origin = (s_holding && s_hold_panel == panel && s_hold_col == col);
    if (is_origin) {
        lv_obj_set_style_bg_color(w, lv_color_hex(0x222222), LV_PART_MAIN);
        lv_obj_t *img = lv_obj_get_child(w, 0);
        if (img) lv_obj_add_flag(img, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_set_style_bg_color(w, lv_color_hex(0x1a2a3a), LV_PART_MAIN);
        lv_obj_t *img = lv_obj_get_child(w, 0);
        if (img) {
            lv_obj_remove_flag(img, LV_OBJ_FLAG_HIDDEN);
            lv_image_set_src(img, st == HD_BOM
                             ? &s_assets[A_HD_BOM].dsc
                             : &s_assets[A_HD_RUIM].dsc);
        }
    }
}

static void refresh_all_slots(void)
{
    for (uint8_t i = 0; i < N_SERVER; i++) refresh_slot(PANEL_SERVER, i);
    for (uint8_t i = 0; i < N_STOCK;  i++) refresh_slot(PANEL_STOCK,  i);
}

static bool check_complete(void)
{
    for (uint8_t i = 0; i < N_SERVER; i++) {
        if (s_srv[i] != HD_BOM) return false;
    }
    return true;
}

static void update_status(void)
{
    if (!s_lbl_status) return;
    uint8_t ruim = 0;
    for (uint8_t i = 0; i < N_SERVER; i++) if (s_srv[i] == HD_RUIM) ruim++;
    char buf[64];
    if (ruim == 0) {
        lv_label_set_text(s_lbl_status, "Todos os HDs OK!");
        lv_obj_set_style_text_color(s_lbl_status, lv_color_hex(0x00C853), LV_PART_MAIN);
    } else {
        snprintf(buf, sizeof(buf), "%u HD(s) com defeito", (unsigned)ruim);
        lv_label_set_text(s_lbl_status, buf);
        lv_obj_set_style_text_color(s_lbl_status, lv_color_hex(0xFF8800), LV_PART_MAIN);
    }
    if (s_lbl_hold) {
        if (s_holding) {
            lv_obj_remove_flag(s_lbl_hold, LV_OBJ_FLAG_HIDDEN);
            lv_label_set_text(s_lbl_hold,
                *slot_state(s_hold_panel, s_hold_col) == HD_RUIM
                ? "Segurando: HD com defeito"
                : "Segurando: HD bom");
        } else {
            lv_obj_add_flag(s_lbl_hold, LV_OBJ_FLAG_HIDDEN);
        }
    }
}

/* ── Tick ────────────────────────────────────────────────────────────────── */

static void tarefa_am_tick(lv_timer_t *t)
{
    (void)t;
    if (!s_overlay) return;

    const joystick_data_t j = joystick_hal_get_state();
    const bool left  = (j.x < -50);
    const bool right = (j.x >  50);
    const bool up    = (j.y < -50);
    const bool down  = (j.y >  50);

    static bool prev_left = false, prev_right = false;
    static bool prev_up   = false, prev_down  = false;
    const bool new_left  = left  && !prev_left;
    const bool new_right = right && !prev_right;
    const bool new_up    = up    && !prev_up;
    const bool new_down  = down  && !prev_down;
    prev_left = left; prev_right = right;
    prev_up   = up;   prev_down  = down;

    const bool a_edge = ui_btn_edge(BTN_A, &s_a_cache);
    const bool b_edge = ui_btn_edge(BTN_B, &s_b_cache);

    if (b_edge) {
        ESP_LOGI(TAG, "B — saindo da tarefa amarela");
        tarefa_am_cb_t cb = s_done_cb;
        screen_tarefa_amarela_destroy();
        if (cb) cb(TAREFA_AM_CANCELADA);
        return;
    }

    /* Modo visualizacao: B fecha, nenhuma interacao permitida. */
    if (s_modo_visualizar) return;

    const uint8_t max_col = panel_max(s_panel);
    bool dirty = false;

    if (new_left  && s_col > 0)            { s_col--; dirty = true; }
    if (new_right && s_col < max_col - 1)  { s_col++; dirty = true; }
    if (new_up || new_down) {
        s_panel = (s_panel == PANEL_SERVER) ? PANEL_STOCK : PANEL_SERVER;
        if (s_col >= panel_max(s_panel)) s_col = panel_max(s_panel) - 1;
        dirty = true;
    }

    if (a_edge) {
        if (!s_holding) {
            /* Pegar o HD deste slot */
            s_holding   = true;
            s_hold_panel = s_panel;
            s_hold_col   = s_col;
            dirty = true;
            ESP_LOGI(TAG, "pegou HD do panel=%d col=%d (%s)",
                     s_panel, s_col,
                     *slot_state(s_hold_panel, s_hold_col) == HD_BOM ? "bom" : "ruim");
        } else {
            /* Soltar — troca com o slot atual */
            if (s_panel == s_hold_panel && s_col == s_hold_col) {
                /* Mesmo slot — cancela carry */
                s_holding = false;
            } else {
                hd_state_t tmp         = *slot_state(s_hold_panel, s_hold_col);
                *slot_state(s_hold_panel, s_hold_col) = *slot_state(s_panel, s_col);
                *slot_state(s_panel, s_col)            = tmp;
                ESP_LOGI(TAG, "trocou panel=%d col=%d <-> panel=%d col=%d",
                         s_hold_panel, s_hold_col, s_panel, s_col);
                s_holding = false;
            }
            dirty = true;

            if (check_complete()) {
                ESP_LOGI(TAG, "tarefa amarela CONCLUIDA");
                refresh_all_slots();
                update_status();
                gamestate_concluir_amarela();
                tarefa_am_cb_t cb = s_done_cb;
                screen_tarefa_amarela_destroy();
                if (cb) cb(TAREFA_AM_CONCLUIDA);
                return;
            }
        }
    }

    if (dirty) {
        refresh_all_slots();
        update_status();
    }
}

/* ── Build / Destroy ─────────────────────────────────────────────────────── */

static void init_state(void)
{
    /* Todos bons no inicio: 6 BAIA + 4 ESTOQUE */
    for (uint8_t i = 0; i < N_SERVER; i++) s_srv[i] = HD_BOM;
    for (uint8_t i = 0; i < N_STOCK;  i++) s_stk[i] = HD_BOM;

    /* 1 a 4 HDs defeituosos aleatorios na BAIA (descricao: max 4 por round) */
    const uint8_t n_ruim = 1 + (uint8_t)(esp_random() % 4);
    uint8_t picked[4] = {0xFF, 0xFF, 0xFF, 0xFF};
    for (uint8_t k = 0; k < n_ruim; k++) {
        uint8_t idx;
        bool clash;
        do {
            idx = (uint8_t)(esp_random() % N_SERVER);
            clash = false;
            for (uint8_t m = 0; m < k; m++) { if (picked[m] == idx) { clash = true; break; } }
        } while (clash);
        s_srv[idx] = HD_RUIM;
        picked[k]  = idx;
    }
    ESP_LOGI(TAG, "estado inicial: %u HD(s) ruim na BAIA", (unsigned)n_ruim);
}

static lv_obj_t *make_slot(lv_obj_t *parent, int16_t x, int16_t y)
{
    lv_obj_t *w = lv_obj_create(parent);
    lv_obj_set_size(w, SLOT_W, SLOT_H);
    lv_obj_set_pos(w, x, y);
    lv_obj_set_style_bg_color(w, lv_color_hex(0x1a2a3a), LV_PART_MAIN);
    lv_obj_set_style_border_color(w, lv_color_hex(0x444444), LV_PART_MAIN);
    lv_obj_set_style_border_width(w, 1, LV_PART_MAIN);
    lv_obj_set_style_pad_all(w, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(w, 0, LV_PART_MAIN);
    no_scroll(w);

    lv_obj_t *img = lv_image_create(w);
    lv_image_set_src(img, &s_assets[A_HD_BOM].dsc);
    lv_obj_center(img);
    no_scroll(img);
    return w;
}

void screen_tarefa_amarela_build(tarefa_am_cb_t done_cb)
{
    if (s_overlay) return;

    const esp_err_t e0 = asset_loader_load(ASSET_TYPE_SPRITE, ASSET_TAREFA_AM_FUNDO,   &s_assets[A_FUNDO]);
    const esp_err_t e1 = asset_loader_load(ASSET_TYPE_SPRITE, ASSET_TAREFA_AM_HD_BOM,  &s_assets[A_HD_BOM]);
    const esp_err_t e2 = asset_loader_load(ASSET_TYPE_SPRITE, ASSET_TAREFA_AM_HD_RUIM, &s_assets[A_HD_RUIM]);
    if (e0 != ESP_OK || e1 != ESP_OK || e2 != ESP_OK) {
        ESP_LOGE(TAG, "assets tarefa amarela indisponiveis");
        asset_loader_free(&s_assets[A_FUNDO]);
        asset_loader_free(&s_assets[A_HD_BOM]);
        asset_loader_free(&s_assets[A_HD_RUIM]);
        return;
    }

    s_done_cb        = done_cb;
    s_modo_visualizar = (gamestate_amarela_estado() == TAREFA_CONCLUIDA);

    if (!s_state_initialized) {
        init_state();
        s_state_initialized = true;
    }

    s_panel   = PANEL_STOCK;
    s_col     = 0;
    s_holding = false;

    s_overlay = lv_obj_create(lv_screen_active());
    lv_obj_set_size(s_overlay, 480, 320);
    lv_obj_set_pos(s_overlay, 0, 0);
    lv_obj_set_style_bg_opa(s_overlay, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_bg_color(s_overlay, lv_color_black(), LV_PART_MAIN);
    lv_obj_set_style_border_width(s_overlay, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(s_overlay, 0, LV_PART_MAIN);
    no_scroll(s_overlay);

    lv_obj_t *bg = lv_image_create(s_overlay);
    lv_image_set_src(bg, &s_assets[A_FUNDO].dsc);
    lv_obj_set_pos(bg, 0, 0);
    no_scroll(bg);

    /* Slots do servidor */
    for (uint8_t i = 0; i < N_SERVER; i++) {
        s_slot_srv[i] = make_slot(s_overlay, SRV_X[i], SRV_Y);
    }
    /* Slots do estoque */
    for (uint8_t i = 0; i < N_STOCK; i++) {
        s_slot_stk[i] = make_slot(s_overlay, STK_X[i], STK_Y);
    }

    /* Status */
    s_lbl_status = lv_label_create(s_overlay);
    lv_obj_set_pos(s_lbl_status, 160, 285);
    lv_obj_set_style_text_color(s_lbl_status, lv_color_hex(0xFF8800), LV_PART_MAIN);
    lv_obj_set_style_bg_color(s_lbl_status, lv_color_black(), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(s_lbl_status, LV_OPA_70, LV_PART_MAIN);
    lv_obj_set_style_pad_all(s_lbl_status, 2, LV_PART_MAIN);
    no_scroll(s_lbl_status);

    /* Indicador de carry */
    s_lbl_hold = lv_label_create(s_overlay);
    lv_obj_set_pos(s_lbl_hold, 4, 4);
    lv_obj_set_style_text_color(s_lbl_hold, lv_color_hex(0xFFD000), LV_PART_MAIN);
    lv_obj_set_style_bg_color(s_lbl_hold, lv_color_black(), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(s_lbl_hold, LV_OPA_70, LV_PART_MAIN);
    lv_obj_set_style_pad_all(s_lbl_hold, 2, LV_PART_MAIN);
    no_scroll(s_lbl_hold);
    lv_obj_add_flag(s_lbl_hold, LV_OBJ_FLAG_HIDDEN);

    s_a_cache = button_hal_peek(BTN_A);
    s_b_cache = button_hal_peek(BTN_B);

    refresh_all_slots();
    update_status();

    /* Banner de conclusao (visivel apenas quando ja concluida). */
    if (s_modo_visualizar) {
        lv_obj_t *banner = lv_obj_create(s_overlay);
        lv_obj_set_size(banner, 480, 36);
        lv_obj_set_pos(banner, 0, 0);
        lv_obj_set_style_bg_color(banner, lv_color_hex(0x00C853), LV_PART_MAIN);
        lv_obj_set_style_border_width(banner, 0, LV_PART_MAIN);
        lv_obj_set_style_pad_all(banner, 4, LV_PART_MAIN);
        lv_obj_remove_flag(banner, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_t *lbl = lv_label_create(banner);
        lv_label_set_text(lbl, "TAREFA AMARELA CONCLUIDA  —  [B] Fechar");
        lv_obj_set_style_text_color(lbl, lv_color_white(), LV_PART_MAIN);
        lv_obj_center(lbl);
    }

    s_timer = lv_timer_create(tarefa_am_tick, UI_TICK_MS, NULL);
    ESP_LOGI(TAG, "tarefa amarela aberta%s", s_modo_visualizar ? " (visualizacao)" : "");
}

void screen_tarefa_amarela_destroy(void)
{
    if (s_timer)   { lv_timer_delete(s_timer); s_timer = NULL; }
    if (s_overlay) { lv_obj_delete(s_overlay); s_overlay = NULL; }
    for (uint8_t i = 0; i < N_SERVER; i++) s_slot_srv[i] = NULL;
    for (uint8_t i = 0; i < N_STOCK;  i++) s_slot_stk[i] = NULL;
    s_lbl_status      = s_lbl_hold = NULL;
    s_modo_visualizar = false;
    asset_loader_free(&s_assets[A_FUNDO]);
    asset_loader_free(&s_assets[A_HD_BOM]);
    asset_loader_free(&s_assets[A_HD_RUIM]);
    s_done_cb = NULL;
    ESP_LOGI(TAG, "tarefa amarela fechada");
}

void screen_tarefa_amarela_reset(void) { s_state_initialized = false; }
bool screen_tarefa_amarela_is_open(void) { return s_overlay != NULL; }
