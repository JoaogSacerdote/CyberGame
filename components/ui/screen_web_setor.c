#include "screen_web_setor.h"
#include "ui_internal.h"

#include <stdint.h>
#include <stdbool.h>
#include "esp_log.h"
#include "lvgl.h"
#include "asset_loader.h"
#include "asset_ids.h"
#include "button_hal.h"
#include "threat.h"
#include "gamestate.h"
#include "fsm.h"

static const char *TAG = "WEB_SETOR";

/* ═══════════════════════════════════════════════════════════════════════════
 * LAYOUT (canvas 480×320 — convenção bottom-center igual ao sistema de
 * entidades da sala: draw_x = bc_x - w/2 ; draw_y = bc_y - h)
 * ═══════════════════════════════════════════════════════════════════════════ */

/* Tela da tarefa é a MESMA para os dois setores.
 * Fonte: Tarefas/VERMELHO/DDOS/POSICAO.txt
 *   SERVIDOR_OK     | 76,291
 *   SERVIDOR_ALERTA | 76,291  (LED overlay, mesmo pivot) */
#define SERVIDOR_BC_X    76
#define SERVIDOR_BC_Y   291

#define LED_BC_X         76
#define LED_BC_Y        291

#define BAR_X           150   /* 230 - 161/2  (bc→tl) */
#define BAR_Y            25   /* 44  - 19     (bc→tl) */
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

#define RANSOM_FADE_STEP      5   /* opa/tick (100 ms) — ~2 s de fade a partir do máximo do Cenário A (89) */

/* ═══════════════════════════════════════════════════════════════════════════
 * ROTA DO ENVELOPE (9 waypoints = 8 segmentos, 2 s totais)
 *
 * Posições top-left do sprite (48×32).
 * Referência POSICAO.txt: ENVELOPE | 281,130 (bc) → draw tl (257, 98).
 * Rota vem da direita em direção ao servidor (bc 76,291).
 * ═══════════════════════════════════════════════════════════════════════════ */

#define N_WAYPOINTS  9
#define N_SEGMENTS   8

static const lv_point_t ROTA_ENVELOPE[N_WAYPOINTS] = {
    {320,  98}, {288,  98}, {262, 101}, {236, 106},
    {211, 115}, {186, 126}, {161, 141}, {136, 161}, {110, 176},
};

/* ═══════════════════════════════════════════════════════════════════════════
 * POSIÇÕES DOS FOGOS — bottom-center direto de INTERACAO.txt
 *
 * PONTO_FOGO_0 bc(114,124)   Layer 01: atrás do servidor
 * PONTO_FOGO_1 bc( 38,124)   Layer 01: atrás do servidor
 * PONTO_FOGO_2 bc(460,202)   Layer 03: rack direito (fundo)
 * PONTO_FOGO_3 bc(357,217)   Layer 03: rack central (fundo)
 * PONTO_FOGO_4 bc(118,271)   Layer 03: frente do servidor
 * ═══════════════════════════════════════════════════════════════════════════ */

#define N_FOGOS  5

static const lv_point_t FOGO_BC[N_FOGOS] = {
    {114, 124}, { 38, 124},               /* Layer 01 */
    {460, 202}, {357, 217}, {118, 271},   /* Layer 03 */
};

/* ═══════════════════════════════════════════════════════════════════════════
 * ESTADO POR INSTÂNCIA
 * ═══════════════════════════════════════════════════════════════════════════ */

typedef enum {
    A_FUNDO = 0,
    A_SRV,
    A_LED,
    A_ENV,
    A_FOGO,
    A_ENV_FOGO,
    A_CHIADO,
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
    button_state_t  btn_b_cache;

    /* ── Módulo 2: estado DDoS ── */
    bool            ddos_ativo;
    uint8_t         progresso;
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
    lv_obj_t       *msgbox;

    loaded_asset_t  assets[A_COUNT];
} web_setor_state_t;

static web_setor_state_t s_inst[WEB_SETOR_COUNT];
static web_setor_carta_cb_t s_carta_cb = NULL;

/* ── Forward declarations ─────────────────────────────────────────────────── */
static void envelope_pos_cb(void *var, int32_t v);
static void envelope_fogo_pos_cb(void *var, int32_t v);
static void led_blink_cb(lv_timer_t *t);
void screen_web_setor_destroy(web_setor_id_t id);
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
    st->fogos_ativos   = 0;
    st->fogo_frame     = 0;
    st->env_fogo_frame = 0;

    ESP_LOGI(TAG, "setor %d: estado base restaurado — DDoS mitigado", (int)st->id);
}

/* ═══════════════════════════════════════════════════════════════════════════
 * MÓDULO 3 — helpers Ransomware
 * ═══════════════════════════════════════════════════════════════════════════ */

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

static void ransom_restore_base_state(web_setor_state_t *st)
{
    if (!st->ransom_ativo) return;

    if (st->msgbox) { lv_obj_delete(st->msgbox); st->msgbox = NULL; }

    if (st->img_chiado) {
        lv_obj_set_style_image_opa(st->img_chiado, LV_OPA_TRANSP, LV_PART_MAIN);
        lv_obj_add_flag(st->img_chiado, LV_OBJ_FLAG_HIDDEN);
    }

    /* Restaurar visuais de ataque: fogos, barra, envelope, LED piscante */
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

    st->ransom_ativo          = false;
    st->ransom_congelado      = false;
    st->ransom_aguarda_backup = false;
    st->ransom_fading         = false;
    st->chiado_opa            = 0;
    st->progresso             = 0;
    st->fogos_ativos          = 0;
    st->fogo_frame            = 0;
    st->env_fogo_frame        = 0;

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
    if (fsm_get_state() == GAME_STATE_PAUSE) return;   /* sala viva sob o pause */

    /* ── BTN_B: fecha msgbox > tela ── */
    if (ui_btn_edge(BTN_B, &st->btn_b_cache)) {
        if (st->msgbox) {
            lv_obj_delete(st->msgbox);
            st->msgbox = NULL;
        } else {
            screen_web_setor_destroy(st->id);
            return;
        }
    }

    /* ── Progressão DDoS ── */
    if (st->ddos_ativo) {
        const uint8_t new_prog = threat_progress_pct((uint8_t)st->id);
        if (new_prog > st->progresso) {
            st->progresso = new_prog;
            lv_bar_set_value(st->bar_progresso, (int32_t)st->progresso, LV_ANIM_OFF);
            update_fogos(st);
            if (st->progresso >= 100) {
                ESP_LOGW(TAG, "setor %d: DDOS BEM-SUCEDIDO — vida do setor perdida",
                         (int)st->id);
            }
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
        if (!st->ransom_congelado) {
            const uint8_t new_prog = threat_progress_pct((uint8_t)st->id);
            if (new_prog > st->progresso) {
                st->progresso = new_prog;
                st->chiado_opa = (uint8_t)((st->progresso * 178U) / 100U);
                if (st->img_chiado) {
                    lv_obj_set_style_image_opa(st->img_chiado, st->chiado_opa, LV_PART_MAIN);
                }
                lv_bar_set_value(st->bar_progresso, (int32_t)st->progresso, LV_ANIM_OFF);
                /* Sem update_fogos(): chamas são exclusivas do DDoS — ransomware
                 * criptografa dados, não gera tráfego/destruição visível. */
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
 *   [06] msgbox                                    (criado sob demanda)
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

    if (ea || eb || ec || ed || ee || ef) {
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
    st->ddos_ativo     = false;
    st->progresso      = 0;
    st->fogos_ativos   = 0;
    st->fogo_frame     = 0;
    st->env_fogo_frame = 0;
    st->timer_fogo     = NULL;
    st->btn_b_cache    = button_hal_peek(BTN_B);

    st->ransom_ativo          = false;
    st->ransom_congelado      = false;
    st->ransom_aguarda_backup = false;
    st->ransom_fading         = false;
    st->chiado_opa            = 0;
    st->img_chiado            = NULL;
    st->msgbox                = NULL;

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
        lv_obj_set_pos(st->img_servidor,
                       SERVIDOR_BC_X - sw / 2,
                       SERVIDOR_BC_Y - sh);
        no_scroll(st->img_servidor);
    }

    /* ─── [02] LED_VERMELHO overlay ─────────────────────────────────────── */
    {
        const int lw = (int)st->assets[A_LED].dsc.header.w;
        const int lh = (int)st->assets[A_LED].dsc.header.h;
        st->img_led = lv_image_create(st->overlay);
        lv_image_set_src(st->img_led, &st->assets[A_LED].dsc);
        lv_obj_set_pos(st->img_led,
                       LED_BC_X - lw / 2,
                       LED_BC_Y - lh);
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
    st->msgbox        = NULL;
    for (int i = 0; i < N_FOGOS; i++) {
        st->fogo_cont[i] = NULL;
        st->fogo_img[i]  = NULL;
    }

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
    st->chiado_opa            = 0;

    /* CHIADO começa transparente e cresce com o progresso */
    if (st->img_chiado) {
        lv_obj_set_style_image_opa(st->img_chiado, LV_OPA_TRANSP, LV_PART_MAIN);
        lv_obj_remove_flag(st->img_chiado, LV_OBJ_FLAG_HIDDEN);
    }

    /* Ransomware: LED vermelho + barra de progresso (criptografia) + CHIADO.
     * Sem chamas e sem envelope-chama — ransomware não gera tráfego de dados
     * visível; o envelope normal continua animando normalmente.
     * A troca de HDs pertence à tarefa amarela (libera a carta de Backup) —
     * a UI de HDs NUNCA aparece nesta tela. */
    if (st->timer_led) { lv_timer_delete(st->timer_led); st->timer_led = NULL; }
    lv_obj_remove_flag(st->img_led, LV_OBJ_FLAG_HIDDEN);
    st->led_visivel = true;

    lv_bar_set_value(st->bar_progresso, 0, LV_ANIM_OFF);
    lv_obj_remove_flag(st->bar_progresso, LV_OBJ_FLAG_HIDDEN);

    /* Tela reaberta com o ransomware JA congelado (isolado >=50% antes):
     * restaura o contexto — barra/chiado no progresso congelado + msgbox
     * pedindo o Backup. Sem isso a UI fresca perdia o estado e o tick
     * (que so atualiza com progresso crescente) deixava tudo em zero. */
    if (threat_is_congelado((uint8_t)id)) {
        st->ransom_congelado      = true;
        st->ransom_aguarda_backup = true;
        st->progresso             = threat_progress_pct((uint8_t)id);
        st->chiado_opa            = (uint8_t)((st->progresso * 178U) / 100U);
        if (st->img_chiado) {
            lv_obj_set_style_image_opa(st->img_chiado, st->chiado_opa, LV_PART_MAIN);
        }
        lv_bar_set_value(st->bar_progresso, (int32_t)st->progresso, LV_ANIM_OFF);
        show_msgbox(st, "Danos severos!\nUse o Backup de Emergencia.");
    }

    ESP_LOGI(TAG, "setor %d: Ransomware INICIADO", (int)id);
}

void screen_web_setor_on_carta(web_setor_id_t id, carta_id_t carta)
{
    if (id >= WEB_SETOR_COUNT || !s_inst[id].open) return;
    web_setor_state_t *st = &s_inst[id];

    ESP_LOGI(TAG, "setor %d: carta recebida id=%d", (int)id, (int)carta);

    /* CARTA_BALANCEAMENTO durante DDoS → mitigar */
    if (st->ddos_ativo && carta == CARTA_BALANCEAMENTO) {
        restore_base_state(st);
        return;
    }

    /* CARTA_ISOLAMENTO durante Ransomware — decisao pelo progresso do MODELO
     * (mesmo valor que threat_mitigate vai usar logo em seguida). */
    if (st->ransom_ativo && carta == CARTA_ISOLAMENTO) {
        if (threat_progress_pct((uint8_t)st->id) < 50) {
            /* Cenário A: progresso volta a zero + fade do CHIADO (~2 s) */
            st->ransom_fading = true;
            st->progresso     = 0;
            lv_bar_set_value(st->bar_progresso, 0, LV_ANIM_OFF);
            ESP_LOGI(TAG, "setor %d: ISOLAMENTO — Cenário A, iniciando fade", (int)st->id);
        } else {
            /* Cenário B: congelar e pedir backup */
            st->ransom_congelado      = true;
            st->ransom_aguarda_backup = true;
            show_msgbox(st, "Danos severos!\nUse o Backup de Emergencia.");
            ESP_LOGW(TAG, "setor %d: ISOLAMENTO — Cenário B, aguardando backup",
                     (int)st->id);
        }
        return;
    }

    /* CARTA_BACKUP após Cenário B — só funciona com todos os HDs do servidor
     * bons (tarefa amarela concluída). Estado de congelamento vem do MODELO,
     * não da flag local (que zera se a tela for fechada e reaberta). */
    if (st->ransom_ativo && carta == CARTA_BACKUP &&
        (st->ransom_aguarda_backup || threat_is_congelado((uint8_t)st->id))) {
        if (gamestate_amarela_estado((uint8_t)st->id) == TAREFA_CONCLUIDA) {
            ESP_LOGI(TAG, "setor %d: BACKUP bem-sucedido — sistema restaurado",
                     (int)st->id);
            ransom_restore_base_state(st);
        } else {
            ESP_LOGW(TAG, "setor %d: BACKUP falhou — HDs danificados", (int)st->id);
            show_msgbox(st, "Backup falhou: HDs danificados!\n"
                            "Troque os HDs do servidor (tarefa amarela)\n"
                            "e tente o Backup novamente.");
        }
        return;
    }

    if (s_carta_cb) s_carta_cb(id, carta);
}

void screen_web_setor_set_carta_cb(web_setor_carta_cb_t cb)
{
    s_carta_cb = cb;
}
