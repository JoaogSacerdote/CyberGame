#include "ui.h"
#include "ui_internal.h"

#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include "esp_log.h"
#include "lvgl.h"
#include "assets.h"
#include "collision_data.h"
#include "joystick_hal.h"
#include "fsm_gameplay.h"
#include "fsm.h"

static const char *TAG = "UI_EMPRESA";

#define PFRAME_W   32
#define PFRAME_H   48
#define PSTEP      2
#define J_DEADZONE 30

static const int8_t WALK_SEQ[] = { 0, 1, 2, 1 };
#define WALK_PERIOD_MS 125

/* Spawn fallback — perto da porta esquerda (entrada vinda da Recepcao).
 * Usado se a tabela AREA_SPAWN nao existir OU se o spawn da tabela cair
 * em cima de um obstaculo. */
#define SPAWN_FALLBACK_X 40
#define SPAWN_FALLBACK_Y 160

static int16_t s_px = SPAWN_FALLBACK_X;
static int16_t s_py = SPAWN_FALLBACK_Y;
static int8_t  s_dir = 1;          /* RIGHT (entra pela porta da esquerda) */
static uint8_t s_walk_idx = 1;
static uint32_t s_walk_ms = 0;

/* Estado do NPC TI: 0=baixo (de costas), 1=direita, 2=cima (de frente) */
static uint8_t s_npc_pose = 0;

static lv_obj_t   *s_root        = NULL;
static lv_obj_t   *s_player      = NULL;
static lv_obj_t   *s_npc         = NULL;
static lv_obj_t   *s_icone_am    = NULL;
static lv_obj_t   *s_icone_vd    = NULL;
static lv_obj_t   *s_lbl_tarefa  = NULL;  /* label flutuante de tarefa verde (simulacao) */
static lv_timer_t *s_timer       = NULL;
static bool        s_in_verde    = false;

#define PCOL_OFF_X 8
#define PCOL_OFF_Y 36
#define PCOL_W     16
#define PCOL_H     12

static bool rects_overlap(int ax, int ay, int aw, int ah,
                          int bx, int by, int bw, int bh)
{
    return (ax < bx + bw) && (ax + aw > bx) &&
           (ay < by + bh) && (ay + ah > by);
}

static bool collides_at(int px, int py)
{
    const int cx = px + PCOL_OFF_X;
    const int cy = py + PCOL_OFF_Y;
    for (size_t i = 0; i < collision_empresa_obstaculos_count; ++i) {
        const collision_rect_t *r = &collision_empresa_obstaculos[i];
        if (rects_overlap(cx, cy, PCOL_W, PCOL_H, r->x, r->y, r->w, r->h)) {
            return true;
        }
    }
    if (cx < 0 || cy < 0 || cx + PCOL_W > 480 || cy + PCOL_H > 320) return true;
    return false;
}

static const collision_rect_t *gatilho_at(int px, int py)
{
    const int cx = px + PCOL_OFF_X;
    const int cy = py + PCOL_OFF_Y;
    for (size_t i = 0; i < collision_empresa_gatilhos_count; ++i) {
        const collision_rect_t *r = &collision_empresa_gatilhos[i];
        if (rects_overlap(cx, cy, PCOL_W, PCOL_H, r->x, r->y, r->w, r->h)) {
            return r;
        }
    }
    return NULL;
}

static void apply_player_frame(void)
{
    const int8_t col = WALK_SEQ[s_walk_idx];
    lv_image_set_offset_x(s_player, -col * PFRAME_W);
    lv_image_set_offset_y(s_player, -s_dir * PFRAME_H);
}

static void set_npc_pose(uint8_t pose)
{
    if (pose == s_npc_pose) return;
    s_npc_pose = pose;
    switch (pose) {
        case 0:
            lv_image_set_src(s_npc, &img_emp_npc_ti_baixo);
            lv_obj_set_pos(s_npc, IMG_EMP_NPC_TI_BAIXO_META.off_x,
                                  IMG_EMP_NPC_TI_BAIXO_META.off_y);
            break;
        case 1:
            lv_image_set_src(s_npc, &img_emp_npc_ti_direita);
            lv_obj_set_pos(s_npc, IMG_EMP_NPC_TI_DIREITA_META.off_x,
                                  IMG_EMP_NPC_TI_DIREITA_META.off_y);
            break;
        case 2:
            lv_image_set_src(s_npc, &img_emp_npc_ti_cima);
            lv_obj_set_pos(s_npc, IMG_EMP_NPC_TI_CIMA_META.off_x,
                                  IMG_EMP_NPC_TI_CIMA_META.off_y);
            break;
        default:
            break;
    }
}

static void empresa_tick(lv_timer_t *t)
{
    (void)t;

    const joystick_data_t j = joystick_hal_get_state();
    int dx = 0, dy = 0;
    if (j.x >  J_DEADZONE) dx = +1;
    else if (j.x < -J_DEADZONE) dx = -1;
    if (j.y >  J_DEADZONE) dy = -1;
    else if (j.y < -J_DEADZONE) dy = +1;

    if (abs(dx) > abs(dy)) {
        s_dir = (dx > 0) ? 1 : 2;
    } else if (dy != 0) {
        s_dir = (dy > 0) ? 0 : 3;
    }

    if (dx != 0) {
        const int nx = s_px + dx * PSTEP;
        if (!collides_at(nx, s_py)) s_px = nx;
    }
    if (dy != 0) {
        const int ny = s_py + dy * PSTEP;
        if (!collides_at(s_px, ny)) s_py = ny;
    }

    if (dx != 0 || dy != 0) {
        s_walk_ms += UI_TICK_MS;
        if (s_walk_ms >= WALK_PERIOD_MS) {
            s_walk_ms = 0;
            s_walk_idx = (s_walk_idx + 1) % (sizeof(WALK_SEQ) / sizeof(WALK_SEQ[0]));
        }
    } else {
        s_walk_idx = 1;
        s_walk_ms = 0;
    }

    lv_obj_set_pos(s_player, s_px, s_py);
    apply_player_frame();

    /* Gatilho NPC TI: muda pose conforme area */
    const collision_rect_t *g = gatilho_at(s_px, s_py);
    if (g && g->kind == AREA_INTERACAO_NPC_TI_DIREITA) {
        set_npc_pose(1);
    } else if (g && g->kind == AREA_INTERACAO_NPC_TI_BAIXO) {
        set_npc_pose(2);
    } else {
        set_npc_pose(0);
    }

    /* Tarefa verde — simulacao com label LVGL flutuante */
    const bool now_verde = (g && g->kind == AREA_TAREFA_VERDE);
    if (now_verde != s_in_verde) {
        s_in_verde = now_verde;
        if (now_verde) {
            lv_obj_remove_flag(s_lbl_tarefa, LV_OBJ_FLAG_HIDDEN);
            ESP_LOGI(TAG, "tarefa verde: player na area");
        } else {
            lv_obj_add_flag(s_lbl_tarefa, LV_OBJ_FLAG_HIDDEN);
        }
    }

    /* Gatilho porta Recepcao -> volta */
    if (g && g->kind == AREA_PORTA_RECEPCAO) {
        ESP_LOGI(TAG, "porta recepcao -> trocando sala");
        fsm_set_gameplay_sala(GAMEPLAY_SALA_RECEPCAO);
    }
}

/* Busca AREA_SPAWN na tabela de gatilhos. Se o spawn da tabela cair em
 * cima de um obstaculo (caso do PAREDES_02 atual que cobre as mesas), usa
 * fallback hardcoded perto da porta esquerda. */
static void apply_spawn_from_table(void)
{
    for (size_t i = 0; i < collision_empresa_gatilhos_count; ++i) {
        const collision_rect_t *r = &collision_empresa_gatilhos[i];
        if (r->kind == AREA_SPAWN) {
            if (!collides_at(r->x, r->y)) {
                s_px = r->x;
                s_py = r->y;
                return;
            }
            ESP_LOGW(TAG, "AREA_SPAWN (%d,%d) cai em obstaculo — fallback (%d,%d)",
                     r->x, r->y, SPAWN_FALLBACK_X, SPAWN_FALLBACK_Y);
            break;
        }
    }
    s_px = SPAWN_FALLBACK_X;
    s_py = SPAWN_FALLBACK_Y;
}

static lv_obj_t *layer_full(lv_obj_t *parent, const lv_image_dsc_t *src,
                            int16_t x, int16_t y)
{
    lv_obj_t *img = lv_image_create(parent);
    lv_image_set_src(img, src);
    lv_obj_set_pos(img, x, y);
    return img;
}

void screen_empresa_build(void)
{
    s_root = lv_obj_create(lv_screen_active());
    lv_obj_set_size(s_root, 480, 320);
    lv_obj_set_pos(s_root, 0, 0);
    lv_obj_set_style_bg_color(s_root, lv_color_black(), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(s_root, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(s_root, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(s_root, 0, LV_PART_MAIN);
    lv_obj_remove_flag(s_root, LV_OBJ_FLAG_SCROLLABLE);

    layer_full(s_root, &img_emp_piso, 0, 0);
    layer_full(s_root, &img_emp_paredes, 0, 0);

    s_player = lv_image_create(s_root);
    lv_image_set_src(s_player, &img_player);
    lv_obj_set_size(s_player, PFRAME_W, PFRAME_H);
    lv_image_set_inner_align(s_player, LV_IMAGE_ALIGN_TOP_LEFT);
    lv_obj_remove_flag(s_player, LV_OBJ_FLAG_SCROLLABLE);
    apply_spawn_from_table();
    s_dir = 1; s_walk_idx = 1; s_walk_ms = 0;
    s_in_verde = false;
    lv_obj_set_pos(s_player, s_px, s_py);
    apply_player_frame();

    layer_full(s_root, &img_emp_complemento,
               IMG_EMP_COMPLEMENTO_META.off_x, IMG_EMP_COMPLEMENTO_META.off_y);

    /* NPC TI — default pose 0 (de costas, sem player perto) */
    s_npc = lv_image_create(s_root);
    s_npc_pose = 255;  /* forca set_npc_pose() executar na primeira chamada */
    set_npc_pose(0);

    /* Icones de tarefa (amarela: NPC TI; verde: PC de tarefa) */
    s_icone_am = lv_image_create(s_root);
    lv_image_set_src(s_icone_am, &img_emp_icone_amarelo);
    lv_obj_set_pos(s_icone_am, IMG_EMP_ICONE_AMARELO_META.off_x,
                               IMG_EMP_ICONE_AMARELO_META.off_y);

    s_icone_vd = lv_image_create(s_root);
    lv_image_set_src(s_icone_vd, &img_emp_icone_verde);
    lv_obj_set_pos(s_icone_vd, IMG_EMP_ICONE_VERDE_META.off_x,
                               IMG_EMP_ICONE_VERDE_META.off_y);

    /* Label flutuante simulando o terminal de tarefa verde (placeholder
     * pra etapa de fontes pixel art + interacao real). Aparece quando
     * player toca a AREA_TAREFA_VERDE. */
    s_lbl_tarefa = lv_label_create(s_root);
    lv_label_set_text(s_lbl_tarefa,
        "[TAREFA VERDE]\n"
        "Trocar senha padrao do PC\n"
        "[A] Aceitar  [B] Cancelar");
    lv_obj_set_style_text_color(s_lbl_tarefa, lv_color_white(), LV_PART_MAIN);
    lv_obj_set_style_bg_color(s_lbl_tarefa, lv_color_black(), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(s_lbl_tarefa, LV_OPA_80, LV_PART_MAIN);
    lv_obj_set_style_pad_all(s_lbl_tarefa, 8, LV_PART_MAIN);
    lv_obj_set_style_border_color(s_lbl_tarefa, lv_color_hex(0x00C853), LV_PART_MAIN);
    lv_obj_set_style_border_width(s_lbl_tarefa, 2, LV_PART_MAIN);
    lv_obj_align(s_lbl_tarefa, LV_ALIGN_TOP_MID, 0, 8);
    lv_obj_add_flag(s_lbl_tarefa, LV_OBJ_FLAG_HIDDEN);

    s_timer = lv_timer_create(empresa_tick, UI_TICK_MS, NULL);
    ESP_LOGI(TAG, "empresa built (player @ %d,%d)", s_px, s_py);
}

void screen_empresa_destroy(void)
{
    if (s_timer) { lv_timer_delete(s_timer); s_timer = NULL; }
    if (s_root)  {
        lv_obj_delete(s_root);
        s_root = NULL;
        s_player = s_npc = s_icone_am = s_icone_vd = s_lbl_tarefa = NULL;
    }
}
