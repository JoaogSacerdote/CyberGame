#include "screen_web_setor.h"
#include "ui_internal.h"

#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include "esp_log.h"
#include "esp_random.h"
#include "lvgl.h"
#include "asset_loader.h"
#include "asset_ids.h"
#include "button_hal.h"
#include "joystick_hal.h"
#include "nfc_config.h"

static const char *TAG = "WEB_SETOR";

/* ═══════════════════════════════════════════════════════════════════════════
 * LAYOUT (canvas 480×320 — convenção bottom-center igual ao sistema de
 * entidades da sala: draw_x = bc_x - w/2 ; draw_y = bc_y - h)
 * ═══════════════════════════════════════════════════════════════════════════ */

#define SERVIDOR_BC_X    76
#define SERVIDOR_BC_Y   291

#define BAR_X           230
#define BAR_Y            44
#define BAR_W           161
#define BAR_H            19

/* ═══════════════════════════════════════════════════════════════════════════
 * SPRITE SHEETS
 *
 * FOGO_DDOS.png     — horizontal, 8 frames × 32×64 px = 256×64 px total
 * ENVELOPE_FOGO.png — vertical,   8 frames × 96×32 px =  96×256 px total
 * ═══════════════════════════════════════════════════════════════════════════ */

#define FOGO_FRAME_W         32
#define FOGO_FRAME_H         64
#define FOGO_FRAME_COUNT      8

#define ENV_FOGO_W           96
#define ENV_FOGO_FRAME_H     32
#define ENV_FOGO_FRAME_COUNT  8
#define ENV_FOGO_REF_X       24

#define FOGO_ANIM_MS         80

/* ═══════════════════════════════════════════════════════════════════════════
 * PROGRESSÃO DOS ATAQUES
 * ═══════════════════════════════════════════════════════════════════════════ */

#define DDOS_BAR_RATE_MS   1000   /* avança 1 %/s */
#define RANSOM_FADE_STEP     18   /* opa/tick (100 ms) — ~1 s de fade a partir do máximo 178 */

/* ═══════════════════════════════════════════════════════════════════════════
 * ROTA DO ENVELOPE (9 waypoints = 8 segmentos, 2 s totais)
 * ═══════════════════════════════════════════════════════════════════════════ */

#define N_WAYPOINTS  9
#define N_SEGMENTS   8

static const lv_point_t ROTA_ENVELOPE[N_WAYPOINTS] = {
    {320, 132}, {288, 132}, {262, 135}, {236, 140},
    {211, 149}, {186, 160}, {161, 175}, {136, 195}, {110, 210},
};

/* ═══════════════════════════════════════════════════════════════════════════
 * POSIÇÕES DOS FOGOS — bottom-center (Layer 01: atrás; Layer 03: na frente)
 * ═══════════════════════════════════════════════════════════════════════════ */

#define N_FOGOS  5

static const lv_point_t FOGO_BC[N_FOGOS] = {
    { 45, 143 }, { 89, 133 },               /* Layer 01 */
    { 50, 195 }, { 48, 291 }, { 94, 296 },  /* Layer 03 */
};

/* ═══════════════════════════════════════════════════════════════════════════
 * UI DE HDs — Módulo 3 (Ransomware)
 *
 * BAIA   : 6 slots, painel superior
 * ESTOQUE: 4 slots, painel inferior
 * ═══════════════════════════════════════════════════════════════════════════ */

#define BAIA_SIZE         6
#define ESTOQUE_SIZE      4
#define WS_HD_SLOT_W     64
#define WS_HD_SLOT_H     82
#define WS_STATUS_X      28
#define WS_STATUS_Y     296
#define BAIA_DRAW_Y      45   /* bc_y=127, h=82 */
#define ESTOQUE_DRAW_Y  195   /* bc_y=277, h=82 */

/* Coordenadas exatas de cada slot (fonte: POSICAO.txt, pivot bottom-center)
 * draw_x = bc_x - WS_HD_SLOT_W/2    draw_y = bc_y - WS_HD_SLOT_H */
static const int16_t BAIA_DRAW_X[BAIA_SIZE]      = {50, 114, 178, 242, 306, 370};
static const int16_t ESTOQUE_DRAW_X[ESTOQUE_SIZE] = {72, 160, 250, 347};

/* ═══════════════════════════════════════════════════════════════════════════
 * ESTADO POR INSTÂNCIA
 * ═══════════════════════════════════════════════════════════════════════════ */

typedef enum { HD_BOM = 0, HD_RUIM } hd_estado_t;

typedef enum {
    A_FUNDO = 0,
    A_SRV,
    A_LED,
    A_ENV,
    A_FOGO,
    A_ENV_FOGO,
    A_CHIADO,
    A_HD_BOM,
    A_HD_RUIM,
    A_COUNT,
} asset_slot_t;

typedef struct {
    bool            open;
    web_setor_id_t  id;

    /* ── Módulo 1: estado base ── */
    lv_obj_t       *overlay;
    lv_obj_t       *img_servidor;
    lv_obj_t       *img_led;
    lv_obj_t       *img_envelope;
    lv_obj_t       *bar_progresso;
    lv_timer_t     *timer_tick;
    lv_timer_t     *timer_led;
    bool            led_visivel;
    bool            aguardando_nfc;
    button_state_t  btn_b_cache;
    button_state_t  btn_y_cache;

    /* ── Módulo 2: estado DDoS ── */
    bool            ddos_ativo;
    uint8_t         progresso;
    uint32_t        bar_tick_count;
    uint8_t         fogos_ativos;
    uint8_t         fogo_frame;
    uint8_t         env_fogo_frame;
    lv_timer_t     *timer_fogo;
    lv_obj_t       *env_fogo_cont;
    lv_obj_t       *env_fogo_img;
    lv_obj_t       *fogo_cont[N_FOGOS];
    lv_obj_t       *fogo_img[N_FOGOS];

    /* ── Módulo 3: estado Ransomware ── */
    bool            ransom_ativo;
    bool            ransom_congelado;      /* true após Cenário B */
    bool            ransom_aguarda_backup; /* true: CARTA_BACKUP esperada */
    bool            ransom_fading;         /* true durante fade-out Cenário A */
    uint8_t         chiado_opa;            /* opacidade atual (0–178) */
    lv_obj_t       *img_chiado;
    hd_estado_t     baia[BAIA_SIZE];
    hd_estado_t     estoque[ESTOQUE_SIZE];
    bool            hd_ui_aberta;
    uint8_t         hd_panel;             /* 0=BAIA  1=ESTOQUE */
    uint8_t         hd_col;
    bool            hd_holding;
    uint8_t         hd_hold_panel;
    uint8_t         hd_hold_col;
    lv_obj_t       *hd_slots_baia[BAIA_SIZE];
    lv_obj_t       *hd_slots_estoque[ESTOQUE_SIZE];
    lv_obj_t       *hd_panel_cont;
    lv_obj_t       *hd_lbl_status;
    lv_obj_t       *msgbox;
    button_state_t  btn_a_cache;
    bool            joy_prev_left, joy_prev_right, joy_prev_up, joy_prev_down;

    loaded_asset_t  assets[A_COUNT];
} web_setor_state_t;

static web_setor_state_t s_inst[WEB_SETOR_COUNT];
static web_setor_carta_cb_t s_carta_cb = NULL;

/* ── Forward declarations ─────────────────────────────────────────────────── */
static void envelope_pos_cb(void *var, int32_t v);
static void envelope_fogo_pos_cb(void *var, int32_t v);
static void led_blink_cb(lv_timer_t *t);
void screen_web_setor_destroy(web_setor_id_t id);
static void close_hd_ui(web_setor_state_t *st);
static void ransom_restore_base_state(web_setor_state_t *st);

/* ═══════════════════════════════════════════════════════════════════════════
 * HELPERS GENÉRICOS
 * ═══════════════════════════════════════════════════════════════════════════ */

static void no_scroll(lv_obj_t *o)
{
    lv_obj_remove_flag(o, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scroll_dir(o, LV_DIR_NONE);
}

/* Container transparente com clipping — usado como viewport de spritesheet. */
static lv_obj_t *make_clip_cont(lv_obj_t *parent, int32_t x, int32_t y,
                                 int32_t w, int32_t h)
{
    lv_obj_t *cont = lv_obj_create(parent);
    lv_obj_set_size(cont, w, h);
    lv_obj_set_pos(cont, x, y);
    lv_obj_set_style_bg_opa(cont, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(cont, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(cont, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(cont, 0, LV_PART_MAIN);
    no_scroll(cont);
    return cont;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * MÓDULO 2 — helpers DDoS
 * ═══════════════════════════════════════════════════════════════════════════ */

static void update_fogos(web_setor_state_t *st)
{
    uint8_t target;
    if      (st->progresso >= 80) target = 5;
    else if (st->progresso >= 60) target = 4;
    else if (st->progresso >= 40) target = 3;
    else if (st->progresso >= 20) target = 2;
    else                          target = 1;

    for (uint8_t i = st->fogos_ativos; i < target; i++) {
        lv_obj_remove_flag(st->fogo_cont[i], LV_OBJ_FLAG_HIDDEN);
        ESP_LOGI(TAG, "setor %d: fogo[%u] visivel", (int)st->id, i);
    }
    st->fogos_ativos = target;
}

static void start_envelope_normal_anim(lv_obj_t *obj)
{
    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, obj);
    lv_anim_set_exec_cb(&a, envelope_pos_cb);
    lv_anim_set_values(&a, 0, N_SEGMENTS * 1000);
    lv_anim_set_duration(&a, 2000);
    lv_anim_set_repeat_count(&a, LV_ANIM_REPEAT_INFINITE);
    lv_anim_set_path_cb(&a, lv_anim_path_linear);
    lv_anim_start(&a);
}

static void start_envelope_fogo_anim(lv_obj_t *obj)
{
    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, obj);
    lv_anim_set_exec_cb(&a, envelope_fogo_pos_cb);
    lv_anim_set_values(&a, 0, N_SEGMENTS * 1000);
    lv_anim_set_duration(&a, 2000);
    lv_anim_set_repeat_count(&a, LV_ANIM_REPEAT_INFINITE);
    lv_anim_set_path_cb(&a, lv_anim_path_linear);
    lv_anim_start(&a);
}

static void restore_base_state(web_setor_state_t *st)
{
    if (!st->ddos_ativo) return;

    if (st->timer_fogo) { lv_timer_delete(st->timer_fogo); st->timer_fogo = NULL; }

    for (int i = 0; i < N_FOGOS; i++) {
        lv_obj_add_flag(st->fogo_cont[i], LV_OBJ_FLAG_HIDDEN);
    }

    lv_bar_set_value(st->bar_progresso, 0, LV_ANIM_OFF);
    lv_obj_add_flag(st->bar_progresso, LV_OBJ_FLAG_HIDDEN);

    lv_anim_delete(st->env_fogo_cont, envelope_fogo_pos_cb);
    lv_obj_add_flag(st->env_fogo_cont, LV_OBJ_FLAG_HIDDEN);

    lv_obj_remove_flag(st->img_envelope, LV_OBJ_FLAG_HIDDEN);
    start_envelope_normal_anim(st->img_envelope);

    st->led_visivel = true;
    lv_obj_remove_flag(st->img_led, LV_OBJ_FLAG_HIDDEN);
    st->timer_led = lv_timer_create(led_blink_cb, 1000, st);

    st->ddos_ativo     = false;
    st->progresso      = 0;
    st->bar_tick_count = 0;
    st->fogos_ativos   = 0;
    st->fogo_frame     = 0;
    st->env_fogo_frame = 0;

    ESP_LOGI(TAG, "setor %d: estado base restaurado — DDoS mitigado", (int)st->id);
}

/* ═══════════════════════════════════════════════════════════════════════════
 * MÓDULO 3 — helpers Ransomware / UI de HDs
 * ═══════════════════════════════════════════════════════════════════════════ */

static bool baia_all_bom(const web_setor_state_t *st)
{
    for (uint8_t i = 0; i < BAIA_SIZE; i++) {
        if (st->baia[i] != HD_BOM) return false;
    }
    return true;
}

static hd_estado_t *hd_ptr(web_setor_state_t *st, uint8_t panel, uint8_t col)
{
    return (panel == 0) ? &st->baia[col] : &st->estoque[col];
}

static void update_hd_status(web_setor_state_t *st)
{
    if (!st->hd_lbl_status) return;
    uint8_t ruim = 0;
    for (uint8_t i = 0; i < BAIA_SIZE; i++) {
        if (st->baia[i] == HD_RUIM) ruim++;
    }
    if (ruim == 0) {
        lv_label_set_text(st->hd_lbl_status, "Todos os HDs OK!");
        lv_obj_set_style_text_color(st->hd_lbl_status, lv_color_hex(0x00C853), LV_PART_MAIN);
    } else {
        char buf[48];
        snprintf(buf, sizeof(buf), "%u HD(s) com defeito na BAIA", (unsigned)ruim);
        lv_label_set_text(st->hd_lbl_status, buf);
        lv_obj_set_style_text_color(st->hd_lbl_status, lv_color_hex(0xFF8800), LV_PART_MAIN);
    }
}

static void refresh_hd_slot(web_setor_state_t *st, uint8_t panel, uint8_t col)
{
    lv_obj_t *w = (panel == 0) ? st->hd_slots_baia[col] : st->hd_slots_estoque[col];
    if (!w) return;

    const bool hovered   = (st->hd_panel == panel && st->hd_col == col);
    const bool is_origin = (st->hd_holding && st->hd_hold_panel == panel
                            && st->hd_hold_col == col);

    lv_color_t border_col;
    if (hovered && is_origin) border_col = lv_color_hex(0xFF8800);
    else if (hovered)         border_col = lv_color_hex(0xFFD000);
    else                      border_col = lv_color_hex(0x444444);

    lv_obj_set_style_border_color(w, border_col, LV_PART_MAIN);
    lv_obj_set_style_border_width(w, hovered ? 3 : 1, LV_PART_MAIN);

    lv_obj_t *img = lv_obj_get_child(w, 0);
    if (!img) return;

    if (is_origin) {
        lv_obj_set_style_bg_color(w, lv_color_hex(0x222222), LV_PART_MAIN);
        lv_obj_add_flag(img, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_set_style_bg_color(w, lv_color_hex(0x1a2a3a), LV_PART_MAIN);
        lv_obj_remove_flag(img, LV_OBJ_FLAG_HIDDEN);
        hd_estado_t estado = *hd_ptr(st, panel, col);
        lv_image_set_src(img, estado == HD_BOM
                         ? &st->assets[A_HD_BOM].dsc
                         : &st->assets[A_HD_RUIM].dsc);
    }
}

static void refresh_hd_all(web_setor_state_t *st)
{
    for (uint8_t i = 0; i < BAIA_SIZE;    i++) refresh_hd_slot(st, 0, i);
    for (uint8_t i = 0; i < ESTOQUE_SIZE; i++) refresh_hd_slot(st, 1, i);
}

static lv_obj_t *make_hd_slot(lv_obj_t *parent, int16_t x, int16_t y,
                               const lv_image_dsc_t *src)
{
    lv_obj_t *w = lv_obj_create(parent);
    lv_obj_set_size(w, WS_HD_SLOT_W, WS_HD_SLOT_H);
    lv_obj_set_pos(w, x, y);
    lv_obj_set_style_bg_color(w, lv_color_hex(0x1a2a3a), LV_PART_MAIN);
    lv_obj_set_style_border_color(w, lv_color_hex(0x444444), LV_PART_MAIN);
    lv_obj_set_style_border_width(w, 1, LV_PART_MAIN);
    lv_obj_set_style_pad_all(w, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(w, 0, LV_PART_MAIN);
    no_scroll(w);

    lv_obj_t *img = lv_image_create(w);
    lv_image_set_src(img, src);
    lv_obj_set_pos(img, 0, 0);
    no_scroll(img);
    return w;
}

static void hd_try_action(web_setor_state_t *st)
{
    if (!st->hd_holding) {
        st->hd_holding    = true;
        st->hd_hold_panel = st->hd_panel;
        st->hd_hold_col   = st->hd_col;
        refresh_hd_slot(st, st->hd_hold_panel, st->hd_hold_col);
    } else if (st->hd_panel == st->hd_hold_panel && st->hd_col == st->hd_hold_col) {
        /* Mesmo slot — cancela carry */
        st->hd_holding = false;
        refresh_hd_slot(st, st->hd_panel, st->hd_col);
    } else {
        /* Swap */
        hd_estado_t tmp = *hd_ptr(st, st->hd_hold_panel, st->hd_hold_col);
        *hd_ptr(st, st->hd_hold_panel, st->hd_hold_col) = *hd_ptr(st, st->hd_panel, st->hd_col);
        *hd_ptr(st, st->hd_panel, st->hd_col)           = tmp;
        uint8_t op = st->hd_hold_panel, oc = st->hd_hold_col;
        st->hd_holding = false;
        refresh_hd_slot(st, op, oc);
        refresh_hd_slot(st, st->hd_panel, st->hd_col);
        update_hd_status(st);
    }
}

static void open_hd_ui(web_setor_state_t *st)
{
    if (st->hd_ui_aberta) return;
    st->hd_ui_aberta = true;
    st->hd_panel     = 0;
    st->hd_col       = 0;
    st->hd_holding   = false;

    st->hd_panel_cont = lv_obj_create(st->overlay);
    lv_obj_set_size(st->hd_panel_cont, 480, 320);
    lv_obj_set_pos(st->hd_panel_cont, 0, 0);
    lv_obj_set_style_bg_opa(st->hd_panel_cont, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(st->hd_panel_cont, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(st->hd_panel_cont, 0, LV_PART_MAIN);
    no_scroll(st->hd_panel_cont);

    lv_obj_t *lbl_b = lv_label_create(st->hd_panel_cont);
    lv_label_set_text(lbl_b, "BAIA:");
    lv_obj_set_pos(lbl_b, BAIA_DRAW_X[0], BAIA_DRAW_Y - 20);
    lv_obj_set_style_text_color(lbl_b, lv_color_hex(0xFFFFFF), LV_PART_MAIN);

    for (uint8_t i = 0; i < BAIA_SIZE; i++) {
        st->hd_slots_baia[i] = make_hd_slot(st->hd_panel_cont,
            BAIA_DRAW_X[i], (int16_t)BAIA_DRAW_Y,
            &st->assets[A_HD_BOM].dsc);
    }

    lv_obj_t *lbl_e = lv_label_create(st->hd_panel_cont);
    lv_label_set_text(lbl_e, "ESTOQUE:");
    lv_obj_set_pos(lbl_e, ESTOQUE_DRAW_X[0], ESTOQUE_DRAW_Y - 20);
    lv_obj_set_style_text_color(lbl_e, lv_color_hex(0xFFFFFF), LV_PART_MAIN);

    for (uint8_t i = 0; i < ESTOQUE_SIZE; i++) {
        st->hd_slots_estoque[i] = make_hd_slot(st->hd_panel_cont,
            ESTOQUE_DRAW_X[i], (int16_t)ESTOQUE_DRAW_Y,
            &st->assets[A_HD_BOM].dsc);
    }

    st->hd_lbl_status = lv_label_create(st->hd_panel_cont);
    lv_obj_set_pos(st->hd_lbl_status, WS_STATUS_X, WS_STATUS_Y);
    lv_obj_set_style_text_color(st->hd_lbl_status, lv_color_hex(0xFF8800), LV_PART_MAIN);

    refresh_hd_all(st);
    update_hd_status(st);
}

static void close_hd_ui(web_setor_state_t *st)
{
    if (!st->hd_ui_aberta) return;
    if (st->hd_panel_cont) {
        lv_obj_delete(st->hd_panel_cont);
        st->hd_panel_cont = NULL;
        st->hd_lbl_status = NULL;
    }
    for (uint8_t i = 0; i < BAIA_SIZE;    i++) st->hd_slots_baia[i]    = NULL;
    for (uint8_t i = 0; i < ESTOQUE_SIZE; i++) st->hd_slots_estoque[i] = NULL;
    st->hd_ui_aberta = false;
    st->hd_holding   = false;
}

static void show_msgbox(web_setor_state_t *st, const char *msg)
{
    if (st->msgbox) { lv_obj_delete(st->msgbox); st->msgbox = NULL; }

    lv_obj_t *box = lv_obj_create(st->overlay);
    lv_obj_set_size(box, 300, 80);
    lv_obj_set_pos(box, (480 - 300) / 2, (320 - 80) / 2);
    lv_obj_set_style_bg_color(box, lv_color_hex(0x1A1A2E), LV_PART_MAIN);
    lv_obj_set_style_border_color(box, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
    lv_obj_set_style_border_width(box, 2, LV_PART_MAIN);
    lv_obj_set_style_pad_all(box, 8, LV_PART_MAIN);
    no_scroll(box);

    lv_obj_t *lbl = lv_label_create(box);
    lv_label_set_text(lbl, msg);
    lv_label_set_long_mode(lbl, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(lbl, 284);
    lv_obj_center(lbl);
    lv_obj_set_style_text_color(lbl, lv_color_hex(0xFFFFFF), LV_PART_MAIN);

    st->msgbox = box;
}

static void handle_hd_ui_input(web_setor_state_t *st)
{
    const joystick_data_t j = joystick_hal_get_state();
    const bool left  = (j.x < -50);
    const bool right = (j.x >  50);
    const bool up    = (j.y < -50);
    const bool down  = (j.y >  50);

    const bool new_left  = left  && !st->joy_prev_left;
    const bool new_right = right && !st->joy_prev_right;
    const bool new_up    = up    && !st->joy_prev_up;
    const bool new_down  = down  && !st->joy_prev_down;

    st->joy_prev_left  = left;
    st->joy_prev_right = right;
    st->joy_prev_up    = up;
    st->joy_prev_down  = down;

    const uint8_t max_col = (st->hd_panel == 0) ? BAIA_SIZE - 1 : ESTOQUE_SIZE - 1;
    bool dirty = false;

    if (new_left  && st->hd_col > 0)        { st->hd_col--; dirty = true; }
    if (new_right && st->hd_col < max_col)  { st->hd_col++; dirty = true; }
    if (new_up || new_down) {
        st->hd_panel = (st->hd_panel == 0) ? 1 : 0;
        const uint8_t new_max = (st->hd_panel == 0) ? BAIA_SIZE - 1 : ESTOQUE_SIZE - 1;
        if (st->hd_col > new_max) st->hd_col = new_max;
        dirty = true;
    }

    if (dirty) refresh_hd_all(st);

    if (ui_btn_edge(BTN_A, &st->btn_a_cache)) {
        hd_try_action(st);
    }
}

static void ransom_restore_base_state(web_setor_state_t *st)
{
    if (!st->ransom_ativo) return;

    if (st->hd_ui_aberta) close_hd_ui(st);
    if (st->msgbox) { lv_obj_delete(st->msgbox); st->msgbox = NULL; }

    if (st->img_chiado) {
        lv_obj_set_style_image_opa(st->img_chiado, LV_OPA_TRANSP, LV_PART_MAIN);
        lv_obj_add_flag(st->img_chiado, LV_OBJ_FLAG_HIDDEN);
    }

    st->ransom_ativo          = false;
    st->ransom_congelado      = false;
    st->ransom_aguarda_backup = false;
    st->ransom_fading         = false;
    st->chiado_opa            = 0;
    st->progresso             = 0;
    st->bar_tick_count        = 0;

    ESP_LOGI(TAG, "setor %d: Ransomware mitigado — estado base restaurado", (int)st->id);
}

/* ═══════════════════════════════════════════════════════════════════════════
 * CALLBACKS DE ANIMAÇÃO (lv_anim exec_cb)
 * ═══════════════════════════════════════════════════════════════════════════ */

static void envelope_pos_cb(void *var, int32_t v)
{
    int seg = v / 1000;
    int t   = v % 1000;
    if (seg >= N_SEGMENTS) { seg = N_SEGMENTS - 1; t = 1000; }
    const int32_t x = ROTA_ENVELOPE[seg].x +
        (ROTA_ENVELOPE[seg + 1].x - ROTA_ENVELOPE[seg].x) * t / 1000;
    const int32_t y = ROTA_ENVELOPE[seg].y +
        (ROTA_ENVELOPE[seg + 1].y - ROTA_ENVELOPE[seg].y) * t / 1000;
    lv_obj_set_pos((lv_obj_t *)var, x, y);
}

static void envelope_fogo_pos_cb(void *var, int32_t v)
{
    int seg = v / 1000;
    int t   = v % 1000;
    if (seg >= N_SEGMENTS) { seg = N_SEGMENTS - 1; t = 1000; }
    const int32_t x = ROTA_ENVELOPE[seg].x +
        (ROTA_ENVELOPE[seg + 1].x - ROTA_ENVELOPE[seg].x) * t / 1000;
    const int32_t y = ROTA_ENVELOPE[seg].y +
        (ROTA_ENVELOPE[seg + 1].y - ROTA_ENVELOPE[seg].y) * t / 1000;
    lv_obj_set_pos((lv_obj_t *)var,
                   x - ENV_FOGO_REF_X,
                   y - ENV_FOGO_FRAME_H / 2);
}

/* ═══════════════════════════════════════════════════════════════════════════
 * CALLBACKS DE TIMER
 * ═══════════════════════════════════════════════════════════════════════════ */

static void led_blink_cb(lv_timer_t *t)
{
    web_setor_state_t *st = (web_setor_state_t *)lv_timer_get_user_data(t);
    if (!st->open || !st->img_led) return;
    st->led_visivel = !st->led_visivel;
    if (st->led_visivel)
        lv_obj_remove_flag(st->img_led, LV_OBJ_FLAG_HIDDEN);
    else
        lv_obj_add_flag(st->img_led, LV_OBJ_FLAG_HIDDEN);
}

static void fogo_anim_cb(lv_timer_t *t)
{
    web_setor_state_t *st = (web_setor_state_t *)lv_timer_get_user_data(t);
    if (!st->open || !st->ddos_ativo) return;

    st->fogo_frame = (uint8_t)((st->fogo_frame + 1) % FOGO_FRAME_COUNT);
    const int32_t fx_off = -((int32_t)st->fogo_frame * FOGO_FRAME_W);
    for (uint8_t i = 0; i < st->fogos_ativos; i++) {
        if (st->fogo_img[i]) lv_obj_set_x(st->fogo_img[i], fx_off);
    }

    st->env_fogo_frame = (uint8_t)((st->env_fogo_frame + 1) % ENV_FOGO_FRAME_COUNT);
    if (st->env_fogo_img) {
        lv_obj_set_y(st->env_fogo_img,
                     -((int32_t)st->env_fogo_frame * ENV_FOGO_FRAME_H));
    }
}

/* Tick de botões e progressão dos ataques @ UI_TICK_MS. */
static void web_setor_tick_cb(lv_timer_t *t)
{
    web_setor_state_t *st = (web_setor_state_t *)lv_timer_get_user_data(t);
    if (!st->open) return;

    /* ── BTN_B: fecha msgbox > hd_ui > tela ── */
    if (ui_btn_edge(BTN_B, &st->btn_b_cache)) {
        if (st->msgbox) {
            lv_obj_delete(st->msgbox);
            st->msgbox = NULL;
        } else if (st->hd_ui_aberta) {
            close_hd_ui(st);
        } else {
            screen_web_setor_destroy(st->id);
            return;
        }
    }

    /* ── BTN_Y: solicitar leitura NFC ── */
    if (ui_btn_edge(BTN_Y, &st->btn_y_cache)) {
        if (!st->aguardando_nfc) {
            st->aguardando_nfc = true;
            ESP_LOGI(TAG, "setor %d: BTN_Y — aguardando carta NFC", (int)st->id);
        }
    }

    /* ── HD UI input (Módulo 3): ativo quando aberta e sem msgbox bloqueando ── */
    if (st->hd_ui_aberta && !st->msgbox) {
        handle_hd_ui_input(st);
    }

    /* ── Progressão DDoS ── */
    if (st->ddos_ativo) {
        st->bar_tick_count++;
        if (st->bar_tick_count < DDOS_BAR_RATE_MS / UI_TICK_MS) return;
        st->bar_tick_count = 0;

        if (st->progresso >= 100) return;
        st->progresso++;
        lv_bar_set_value(st->bar_progresso, (int32_t)st->progresso, LV_ANIM_OFF);
        update_fogos(st);

        if (st->progresso >= 100) {
            ESP_LOGW(TAG, "setor %d: DDOS BEM-SUCEDIDO — vida do setor perdida",
                     (int)st->id);
        }
        return;
    }

    /* ── Progressão Ransomware ── */
    if (st->ransom_ativo) {
        /* Cenário A: fade-out em andamento */
        if (st->ransom_fading) {
            if (st->chiado_opa > RANSOM_FADE_STEP) {
                st->chiado_opa -= RANSOM_FADE_STEP;
            } else {
                st->chiado_opa    = 0;
                st->ransom_fading = false;
                ransom_restore_base_state(st);
                return;
            }
            if (st->img_chiado) {
                lv_obj_set_style_image_opa(st->img_chiado, st->chiado_opa, LV_PART_MAIN);
            }
            return;
        }

        /* Progressão normal (congelado = parado em Cenário B) */
        if (!st->ransom_congelado && st->progresso < 100) {
            st->bar_tick_count++;
            if (st->bar_tick_count >= DDOS_BAR_RATE_MS / UI_TICK_MS) {
                st->bar_tick_count = 0;
                st->progresso++;
                st->chiado_opa = (uint8_t)((st->progresso * 178U) / 100U);
                if (st->img_chiado) {
                    lv_obj_set_style_image_opa(st->img_chiado, st->chiado_opa, LV_PART_MAIN);
                }
                if (st->progresso >= 100) {
                    ESP_LOGW(TAG, "setor %d: RANSOMWARE MÁXIMO — vida do setor perdida",
                             (int)st->id);
                }
            }
        }
    }
}

/* ═══════════════════════════════════════════════════════════════════════════
 * BUILD — cria todos os objetos LVGL na ordem de z correta
 *
 * Z-order (bottom → top):
 *   [00] Fundo
 *   [01] Fogo layer-01: fogo[0], fogo[1]          (atrás do servidor)
 *   [02] Servidor + LED overlay + envelopes        (plano do servidor)
 *   [03] Fogo layer-03: fogo[2..4]                 (na frente do servidor)
 *   [04] Barra de progresso                        (UI DDoS)
 *   [05] CHIADO                                    (UI Ransomware)
 *   [06+] hd_panel_cont, msgbox                   (criados sob demanda)
 * ═══════════════════════════════════════════════════════════════════════════ */

void screen_web_setor_build(web_setor_id_t id)
{
    if (id >= WEB_SETOR_COUNT) return;
    web_setor_state_t *st = &s_inst[id];
    if (st->open) return;

    /* ── Carregar assets críticos ── */
    const esp_err_t ea = asset_loader_load(ASSET_TYPE_SPRITE, ASSET_TAREFA_VM_FUNDO,       &st->assets[A_FUNDO]);
    const esp_err_t eb = asset_loader_load(ASSET_TYPE_SPRITE, ASSET_TAREFA_VM_SERVIDOR_OK, &st->assets[A_SRV]);
    const esp_err_t ec = asset_loader_load(ASSET_TYPE_SPRITE, ASSET_TAREFA_VM_SERVIDOR_AL, &st->assets[A_LED]);
    const esp_err_t ed = asset_loader_load(ASSET_TYPE_SPRITE, ASSET_TAREFA_VM_ENVELOPE,    &st->assets[A_ENV]);
    const esp_err_t ee = asset_loader_load(ASSET_TYPE_SPRITE, ASSET_TAREFA_VM_FOGO,        &st->assets[A_FOGO]);
    const esp_err_t ef = asset_loader_load(ASSET_TYPE_SPRITE, ASSET_TAREFA_VM_ENV_FOGO,    &st->assets[A_ENV_FOGO]);
    const esp_err_t eh = asset_loader_load(ASSET_TYPE_SPRITE, ASSET_TAREFA_AM_HD_BOM,      &st->assets[A_HD_BOM]);
    const esp_err_t ei = asset_loader_load(ASSET_TYPE_SPRITE, ASSET_TAREFA_AM_HD_RUIM,     &st->assets[A_HD_RUIM]);

    if (ea || eb || ec || ed || ee || ef || eh || ei) {
        ESP_LOGE(TAG, "setor %d: assets criticos indisponiveis no SD", (int)id);
        for (int i = 0; i < A_COUNT; i++) asset_loader_free(&st->assets[i]);
        return;
    }

    /* ── CHIADO: asset opcional (Módulo 3) ── */
    const bool chiado_ok = (asset_loader_load(ASSET_TYPE_SPRITE, ASSET_CHIADO,
                                              &st->assets[A_CHIADO]) == ESP_OK);
    if (!chiado_ok) {
        ESP_LOGW(TAG, "setor %d: CHIADO.png ausente — visual de ransomware indisponivel",
                 (int)id);
    }

    /* ── Inicializar estado ── */
    st->open           = true;
    st->id             = id;
    st->led_visivel    = true;
    st->aguardando_nfc = false;
    st->ddos_ativo     = false;
    st->progresso      = 0;
    st->bar_tick_count = 0;
    st->fogos_ativos   = 0;
    st->fogo_frame     = 0;
    st->env_fogo_frame = 0;
    st->timer_fogo     = NULL;
    st->btn_b_cache    = button_hal_peek(BTN_B);
    st->btn_y_cache    = button_hal_peek(BTN_Y);
    st->btn_a_cache    = button_hal_peek(BTN_A);

    st->ransom_ativo          = false;
    st->ransom_congelado      = false;
    st->ransom_aguarda_backup = false;
    st->ransom_fading         = false;
    st->chiado_opa            = 0;
    st->img_chiado            = NULL;
    st->hd_ui_aberta          = false;
    st->hd_panel              = 0;
    st->hd_col                = 0;
    st->hd_holding            = false;
    st->hd_panel_cont         = NULL;
    st->hd_lbl_status         = NULL;
    st->msgbox                = NULL;
    st->joy_prev_left = st->joy_prev_right = st->joy_prev_up = st->joy_prev_down = false;
    for (uint8_t i = 0; i < BAIA_SIZE;    i++) { st->baia[i]   = HD_BOM; st->hd_slots_baia[i]    = NULL; }
    for (uint8_t i = 0; i < ESTOQUE_SIZE; i++) { st->estoque[i] = HD_BOM; st->hd_slots_estoque[i] = NULL; }

    /* ── Overlay 480×320 ── */
    st->overlay = lv_obj_create(lv_screen_active());
    lv_obj_set_size(st->overlay, 480, 320);
    lv_obj_set_pos(st->overlay, 0, 0);
    lv_obj_set_style_pad_all(st->overlay, 0, LV_PART_MAIN);
    lv_obj_set_style_border_width(st->overlay, 0, LV_PART_MAIN);
    no_scroll(st->overlay);

    /* ─── [00] SERVIDOR_FUNDO ────────────────────────────────────────────── */
    lv_obj_t *bg = lv_image_create(st->overlay);
    lv_image_set_src(bg, &st->assets[A_FUNDO].dsc);
    lv_obj_set_pos(bg, 0, 0);
    no_scroll(bg);

    /* ─── [01] Fogos Layer-01 (atrás do servidor) ───────────────────────── */
    for (int i = 0; i < 2; i++) {
        const int32_t fx = (int32_t)FOGO_BC[i].x - FOGO_FRAME_W / 2;
        const int32_t fy = (int32_t)FOGO_BC[i].y - FOGO_FRAME_H;
        st->fogo_cont[i] = make_clip_cont(st->overlay, fx, fy,
                                          FOGO_FRAME_W, FOGO_FRAME_H);
        lv_obj_add_flag(st->fogo_cont[i], LV_OBJ_FLAG_HIDDEN);

        st->fogo_img[i] = lv_image_create(st->fogo_cont[i]);
        lv_image_set_src(st->fogo_img[i], &st->assets[A_FOGO].dsc);
        lv_obj_set_pos(st->fogo_img[i], 0, 0);
        no_scroll(st->fogo_img[i]);
    }

    /* ─── [02] Servidor ─────────────────────────────────────────────────── */
    {
        const int sw = (int)st->assets[A_SRV].dsc.header.w;
        const int sh = (int)st->assets[A_SRV].dsc.header.h;
        st->img_servidor = lv_image_create(st->overlay);
        lv_image_set_src(st->img_servidor, &st->assets[A_SRV].dsc);
        lv_obj_set_pos(st->img_servidor, SERVIDOR_BC_X - sw / 2, SERVIDOR_BC_Y - sh);
        no_scroll(st->img_servidor);
    }

    /* ─── [02] LED_VERMELHO overlay ─────────────────────────────────────── */
    {
        const int lw = (int)st->assets[A_LED].dsc.header.w;
        const int lh = (int)st->assets[A_LED].dsc.header.h;
        st->img_led = lv_image_create(st->overlay);
        lv_image_set_src(st->img_led, &st->assets[A_LED].dsc);
        lv_obj_set_pos(st->img_led, SERVIDOR_BC_X - lw / 2, SERVIDOR_BC_Y - lh);
        no_scroll(st->img_led);
    }

    /* ─── [02] Envelope normal ──────────────────────────────────────────── */
    st->img_envelope = lv_image_create(st->overlay);
    lv_image_set_src(st->img_envelope, &st->assets[A_ENV].dsc);
    lv_obj_set_pos(st->img_envelope, ROTA_ENVELOPE[0].x, ROTA_ENVELOPE[0].y);
    no_scroll(st->img_envelope);

    /* ─── [02] Envelope fogo (oculto até DDoS) ──────────────────────────── */
    st->env_fogo_cont = make_clip_cont(st->overlay,
                                       ROTA_ENVELOPE[0].x - ENV_FOGO_REF_X,
                                       ROTA_ENVELOPE[0].y - ENV_FOGO_FRAME_H / 2,
                                       ENV_FOGO_W, ENV_FOGO_FRAME_H);
    lv_obj_add_flag(st->env_fogo_cont, LV_OBJ_FLAG_HIDDEN);

    st->env_fogo_img = lv_image_create(st->env_fogo_cont);
    lv_image_set_src(st->env_fogo_img, &st->assets[A_ENV_FOGO].dsc);
    lv_obj_set_pos(st->env_fogo_img, 0, 0);
    no_scroll(st->env_fogo_img);

    /* ─── [03] Fogos Layer-03 (na frente do servidor) ───────────────────── */
    for (int i = 2; i < N_FOGOS; i++) {
        const int32_t fx = (int32_t)FOGO_BC[i].x - FOGO_FRAME_W / 2;
        const int32_t fy = (int32_t)FOGO_BC[i].y - FOGO_FRAME_H;
        st->fogo_cont[i] = make_clip_cont(st->overlay, fx, fy,
                                          FOGO_FRAME_W, FOGO_FRAME_H);
        lv_obj_add_flag(st->fogo_cont[i], LV_OBJ_FLAG_HIDDEN);

        st->fogo_img[i] = lv_image_create(st->fogo_cont[i]);
        lv_image_set_src(st->fogo_img[i], &st->assets[A_FOGO].dsc);
        lv_obj_set_pos(st->fogo_img[i], 0, 0);
        no_scroll(st->fogo_img[i]);
    }

    /* ─── [04] Barra de progresso (DDoS) ────────────────────────────────── */
    st->bar_progresso = lv_bar_create(st->overlay);
    lv_obj_set_size(st->bar_progresso, BAR_W, BAR_H);
    lv_obj_set_pos(st->bar_progresso, BAR_X, BAR_Y);
    lv_bar_set_range(st->bar_progresso, 0, 100);
    lv_bar_set_value(st->bar_progresso, 0, LV_ANIM_OFF);
    lv_obj_set_style_bg_color(st->bar_progresso, lv_color_hex(0x1A1A2E), LV_PART_MAIN);
    lv_obj_set_style_bg_color(st->bar_progresso, lv_color_hex(0xFF4040), LV_PART_INDICATOR);
    lv_obj_add_flag(st->bar_progresso, LV_OBJ_FLAG_HIDDEN);

    /* ─── [05] CHIADO (Módulo 3 — Ransomware) ───────────────────────────── */
    st->img_chiado = lv_image_create(st->overlay);
    if (chiado_ok) {
        lv_image_set_src(st->img_chiado, &st->assets[A_CHIADO].dsc);
    }
    lv_obj_set_pos(st->img_chiado, 0, 0);
    lv_obj_set_style_image_opa(st->img_chiado, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_add_flag(st->img_chiado, LV_OBJ_FLAG_HIDDEN);
    no_scroll(st->img_chiado);

    /* ── Animação do envelope normal (estado base) ── */
    start_envelope_normal_anim(st->img_envelope);

    st->timer_led  = lv_timer_create(led_blink_cb,      1000,       st);
    st->timer_tick = lv_timer_create(web_setor_tick_cb, UI_TICK_MS, st);

    ESP_LOGI(TAG, "web setor %d aberto", (int)id);
}

/* ═══════════════════════════════════════════════════════════════════════════
 * DESTROY
 * ═══════════════════════════════════════════════════════════════════════════ */

void screen_web_setor_destroy(web_setor_id_t id)
{
    if (id >= WEB_SETOR_COUNT) return;
    web_setor_state_t *st = &s_inst[id];
    if (!st->open) return;

    if (st->timer_tick) { lv_timer_delete(st->timer_tick); st->timer_tick = NULL; }
    if (st->timer_led)  { lv_timer_delete(st->timer_led);  st->timer_led  = NULL; }
    if (st->timer_fogo) { lv_timer_delete(st->timer_fogo); st->timer_fogo = NULL; }

    if (st->img_envelope)  lv_anim_delete(st->img_envelope,  envelope_pos_cb);
    if (st->env_fogo_cont) lv_anim_delete(st->env_fogo_cont, envelope_fogo_pos_cb);

    if (st->overlay) { lv_obj_delete(st->overlay); st->overlay = NULL; }

    /* Nulificar ponteiros de filhos (todos invalidados pelo delete do overlay) */
    st->img_servidor  = NULL;
    st->img_led       = NULL;
    st->img_envelope  = NULL;
    st->env_fogo_cont = NULL;
    st->env_fogo_img  = NULL;
    st->bar_progresso = NULL;
    st->img_chiado    = NULL;
    st->hd_panel_cont = NULL;
    st->hd_lbl_status = NULL;
    st->msgbox        = NULL;
    for (int i = 0; i < N_FOGOS; i++) {
        st->fogo_cont[i] = NULL;
        st->fogo_img[i]  = NULL;
    }
    for (uint8_t i = 0; i < BAIA_SIZE;    i++) st->hd_slots_baia[i]    = NULL;
    for (uint8_t i = 0; i < ESTOQUE_SIZE; i++) st->hd_slots_estoque[i] = NULL;
    st->hd_ui_aberta = false;

    for (int i = 0; i < A_COUNT; i++) asset_loader_free(&st->assets[i]);

    st->open = false;
    ESP_LOGI(TAG, "web setor %d fechado", (int)id);
}

/* ═══════════════════════════════════════════════════════════════════════════
 * API PÚBLICA
 * ═══════════════════════════════════════════════════════════════════════════ */

bool screen_web_setor_is_open(web_setor_id_t id)
{
    if (id >= WEB_SETOR_COUNT) return false;
    return s_inst[id].open;
}

void screen_web_setor_ddos_start(web_setor_id_t id)
{
    if (id >= WEB_SETOR_COUNT || !s_inst[id].open) return;
    web_setor_state_t *st = &s_inst[id];
    if (st->ddos_ativo || st->ransom_ativo) return;

    st->ddos_ativo     = true;
    st->progresso      = 0;
    st->bar_tick_count = 0;
    st->fogos_ativos   = 0;
    st->fogo_frame     = 0;
    st->env_fogo_frame = 0;

    if (st->timer_led) { lv_timer_delete(st->timer_led); st->timer_led = NULL; }
    lv_obj_remove_flag(st->img_led, LV_OBJ_FLAG_HIDDEN);
    st->led_visivel = true;

    lv_anim_delete(st->img_envelope, envelope_pos_cb);
    lv_obj_add_flag(st->img_envelope, LV_OBJ_FLAG_HIDDEN);

    lv_obj_remove_flag(st->env_fogo_cont, LV_OBJ_FLAG_HIDDEN);
    start_envelope_fogo_anim(st->env_fogo_cont);

    lv_bar_set_value(st->bar_progresso, 0, LV_ANIM_OFF);
    lv_obj_remove_flag(st->bar_progresso, LV_OBJ_FLAG_HIDDEN);

    update_fogos(st);
    st->timer_fogo = lv_timer_create(fogo_anim_cb, FOGO_ANIM_MS, st);

    ESP_LOGI(TAG, "setor %d: DDoS INICIADO", (int)id);
}

void screen_web_setor_ransomware_start(web_setor_id_t id)
{
    if (id >= WEB_SETOR_COUNT || !s_inst[id].open) return;
    web_setor_state_t *st = &s_inst[id];
    if (st->ddos_ativo || st->ransom_ativo) return;

    st->ransom_ativo          = true;
    st->ransom_congelado      = false;
    st->ransom_aguarda_backup = false;
    st->ransom_fading         = false;
    st->progresso             = 0;
    st->bar_tick_count        = 0;
    st->chiado_opa            = 0;

    /* Inicializar HDs: todos bons, depois quebrar 1–4 na BAIA */
    for (uint8_t i = 0; i < BAIA_SIZE;    i++) st->baia[i]   = HD_BOM;
    for (uint8_t i = 0; i < ESTOQUE_SIZE; i++) st->estoque[i] = HD_BOM;

    const uint8_t n_ruim = 1 + (uint8_t)(esp_random() % 4);
    uint8_t picked[4] = {0xFF, 0xFF, 0xFF, 0xFF};
    for (uint8_t k = 0; k < n_ruim; k++) {
        uint8_t idx;
        bool clash;
        do {
            idx   = (uint8_t)(esp_random() % BAIA_SIZE);
            clash = false;
            for (uint8_t m = 0; m < k; m++) {
                if (picked[m] == idx) { clash = true; break; }
            }
        } while (clash);
        st->baia[idx] = HD_RUIM;
        picked[k]     = idx;
    }
    ESP_LOGI(TAG, "setor %d: Ransomware INICIADO — %u HD(s) corrompidos na BAIA",
             (int)id, (unsigned)n_ruim);

    /* CHIADO começa transparente e cresce com o progresso */
    if (st->img_chiado) {
        lv_obj_set_style_image_opa(st->img_chiado, LV_OPA_TRANSP, LV_PART_MAIN);
        lv_obj_remove_flag(st->img_chiado, LV_OBJ_FLAG_HIDDEN);
    }

    open_hd_ui(st);
}

void screen_web_setor_on_carta(web_setor_id_t id, carta_id_t carta)
{
    if (id >= WEB_SETOR_COUNT || !s_inst[id].open) return;
    web_setor_state_t *st = &s_inst[id];

    st->aguardando_nfc = false;
    ESP_LOGI(TAG, "setor %d: carta recebida id=%d", (int)id, (int)carta);

    /* CARTA_BALANCEAMENTO durante DDoS → mitigar */
    if (st->ddos_ativo && carta == CARTA_BALANCEAMENTO) {
        restore_base_state(st);
        return;
    }

    /* CARTA_ISOLAMENTO durante Ransomware */
    if (st->ransom_ativo && carta == CARTA_ISOLAMENTO) {
        if (st->progresso < 50) {
            /* Cenário A: fade automático */
            st->ransom_fading = true;
            ESP_LOGI(TAG, "setor %d: ISOLAMENTO — Cenário A, iniciando fade", (int)st->id);
        } else {
            /* Cenário B: congelar e pedir backup */
            st->ransom_congelado      = true;
            st->ransom_aguarda_backup = true;
            show_msgbox(st, "Danos Severos.\nAperte Y e use o Backup.");
            ESP_LOGW(TAG, "setor %d: ISOLAMENTO — Cenário B, aguardando backup",
                     (int)st->id);
        }
        return;
    }

    /* CARTA_BACKUP após Cenário B */
    if (st->ransom_aguarda_backup && carta == CARTA_BACKUP) {
        if (baia_all_bom(st)) {
            ESP_LOGI(TAG, "setor %d: BACKUP bem-sucedido — todos HDs restaurados",
                     (int)st->id);
            ransom_restore_base_state(st);
        } else {
            ESP_LOGE(TAG, "setor %d: BACKUP falhou — ainda ha HDs corrompidos na BAIA",
                     (int)st->id);
        }
        return;
    }

    if (s_carta_cb) s_carta_cb(id, carta);
}

void screen_web_setor_set_carta_cb(web_setor_carta_cb_t cb)
{
    s_carta_cb = cb;
}
