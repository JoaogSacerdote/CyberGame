#include "ui.h"
#include "ui_internal.h"

#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include "esp_log.h"
#include "lvgl.h"
#include "asset_loader.h"
#include "asset_ids.h"
#include "collision_data.h"
#include "joystick_hal.h"
#include "button_hal.h"
#include "fsm_gameplay.h"
#include "fsm.h"

static const char *TAG = "UI_EMPRESA";

#define PFRAME_W   32
#define PFRAME_H   48
#define J_DEADZONE 30
#define PSTEP_MIN  2                /* -20% do valor anterior */
#define PSTEP_MAX  6                /* -20% do valor anterior */

static const int8_t WALK_SEQ[] = { 0, 1, 2, 1 };
#define WALK_PERIOD_MS 125

/* Velocidade proporcional a deflexao do joystick. */
static int speed_from_mag(int mag)
{
    if (mag <= J_DEADZONE) return 0;
    int s = PSTEP_MIN + (mag - J_DEADZONE) * (PSTEP_MAX - PSTEP_MIN) / (100 - J_DEADZONE);
    if (s < PSTEP_MIN) s = PSTEP_MIN;
    if (s > PSTEP_MAX) s = PSTEP_MAX;
    return s;
}

/* Spawn fallback — perto da porta esquerda (entrada vinda da Recepcao).
 * Usado se a tabela AREA_SPAWN nao existir OU se o spawn da tabela cair
 * em cima de um obstaculo. */
#define SPAWN_FALLBACK_X 40
#define SPAWN_FALLBACK_Y 160

static int16_t s_px = SPAWN_FALLBACK_X;
static int16_t s_py = SPAWN_FALLBACK_Y;
static int8_t  s_dir = 2;          /* linha do sheet 0=DOWN 1=LEFT 2=RIGHT 3=UP — entra olhando pra RIGHT */
static uint8_t s_walk_idx = 1;
static uint32_t s_walk_ms = 0;

/* Pose do NPC TI (indices batem com os arquivos):
 *   0 = PARA_BAIXO  (de frente, encarando o player que vem por baixo)
 *   1 = PARA_DIREITA (perfil direito)
 *   2 = PARA_CIMA   (de costas — pose default, trabalhando no PC) */
static uint8_t s_npc_pose = 0;

static lv_obj_t   *s_root        = NULL;
static lv_obj_t   *s_player      = NULL;
static lv_obj_t   *s_npc         = NULL;
static lv_obj_t   *s_icone_am    = NULL;
static lv_obj_t   *s_icone_vd    = NULL;
static lv_obj_t   *s_prompt      = NULL;  /* "[A]" sobre o player perto de gatilho */
static lv_obj_t   *s_lbl_tarefa  = NULL;  /* label flutuante de tarefa verde (simulacao) */
static lv_timer_t *s_timer       = NULL;
static bool        s_tarefa_open = false; /* label de tarefa verde aberto */
static bool        s_porta_armed = false; /* anti-loop ao spawnar perto de porta */
static button_state_t s_a_cache  = BTN_RELEASED;
static button_state_t s_b_cache  = BTN_RELEASED;

/* === Assets da tela, carregados da NAND no build e liberados no destroy === */
typedef enum {
    A_PISO = 0, A_PAREDES, A_PLAYER, A_COMPLEMENTO,
    A_NPC_BAIXO, A_NPC_DIREITA, A_NPC_CIMA, A_ICONE_AM, A_ICONE_VD,
    A_COUNT
} emp_slot_t;

static const uint16_t EMP_ASSET_ID[A_COUNT] = {
    [A_PISO]        = ASSET_EMP_PISO,
    [A_PAREDES]     = ASSET_EMP_PAREDES,
    [A_PLAYER]      = ASSET_PLAYER,
    [A_COMPLEMENTO] = ASSET_EMP_COMPLEMENTO,
    [A_NPC_BAIXO]   = ASSET_EMP_NPC_TI_BAIXO,
    [A_NPC_DIREITA] = ASSET_EMP_NPC_TI_DIREITA,
    [A_NPC_CIMA]    = ASSET_EMP_NPC_TI_CIMA,
    [A_ICONE_AM]    = ASSET_EMP_ICONE_AMARELO,
    [A_ICONE_VD]    = ASSET_EMP_ICONE_VERDE,
};
static loaded_asset_t s_assets[A_COUNT];

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
    if (!s_npc) return;
    if (pose == s_npc_pose) return;
    s_npc_pose = pose;
    switch (pose) {
        case 0:
            lv_image_set_src(s_npc, &s_assets[A_NPC_BAIXO].dsc);
            lv_obj_set_pos(s_npc, s_assets[A_NPC_BAIXO].off_x,
                                  s_assets[A_NPC_BAIXO].off_y);
            break;
        case 1:
            lv_image_set_src(s_npc, &s_assets[A_NPC_DIREITA].dsc);
            lv_obj_set_pos(s_npc, s_assets[A_NPC_DIREITA].off_x,
                                  s_assets[A_NPC_DIREITA].off_y);
            break;
        case 2:
            lv_image_set_src(s_npc, &s_assets[A_NPC_CIMA].dsc);
            lv_obj_set_pos(s_npc, s_assets[A_NPC_CIMA].off_x,
                                  s_assets[A_NPC_CIMA].off_y);
            break;
        default:
            break;
    }
}

static void empresa_tick(lv_timer_t *t)
{
    (void)t;

    /* Guard defensivo: tela destruida mas timer disparou. */
    if (!s_root || !s_player) return;

    /* O eixo X chega invertido do joystick_hal; invertemos so o X. jx>0=direita, jy>0=baixo. */
    const joystick_data_t j = joystick_hal_get_state();
    const int jx = -j.x;
    const int jy = j.y;
    int dx = 0, dy = 0, sx = 0, sy = 0;
    if (jx >  J_DEADZONE) { dx = +1; sx = speed_from_mag(jx); }
    else if (jx < -J_DEADZONE) { dx = -1; sx = speed_from_mag(-jx); }
    if (jy >  J_DEADZONE) { dy = +1; sy = speed_from_mag(jy); }
    else if (jy < -J_DEADZONE) { dy = -1; sy = speed_from_mag(-jy); }

    /* Linhas do sheet: 0=DOWN, 1=LEFT, 2=RIGHT, 3=UP. */
    if (abs(jx) > abs(jy)) {
        if (dx != 0) s_dir = (dx > 0) ? 2 /*RIGHT*/ : 1 /*LEFT*/;
    } else if (dy != 0) {
        s_dir = (dy > 0) ? 0 /*DOWN*/ : 3 /*UP*/;
    }

    if (dx != 0) {
        const int nx = s_px + dx * sx;
        if (!collides_at(nx, s_py)) s_px = nx;
    }
    if (dy != 0) {
        const int ny = s_py + dy * sy;
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

    /* Gatilho sob o player */
    const collision_rect_t *g = gatilho_at(s_px, s_py);

    /* NPC TI muda pose conforme area (feedback visual — automatico).
     * Player na AREA_BAIXO -> NPC encara pra baixo (pose 0).
     * Player na AREA_DIREITA -> NPC vira pra direita (pose 1).
     * Player longe -> NPC de costas trabalhando (pose 2). */
    if (g && g->kind == AREA_INTERACAO_NPC_TI_DIREITA) {
        set_npc_pose(1);
    } else if (g && g->kind == AREA_INTERACAO_NPC_TI_BAIXO) {
        set_npc_pose(0);
    } else {
        set_npc_pose(2);
    }

    /* Porta: troca de sala por CONTATO. s_porta_armed evita loop quando o
     * player spawna perto da porta. */
    if (g && g->kind == AREA_PORTA_RECEPCAO) {
        if (s_porta_armed) {
            ESP_LOGI(TAG, "porta recepcao (contato) -> trocando sala");
            fsm_set_gameplay_sala(GAMEPLAY_SALA_RECEPCAO);
            return;
        }
    } else if (!g || g->kind != AREA_PORTA_RECEPCAO) {
        s_porta_armed = true;  /* saiu da porta -> rearma */
    }

    /* Prompt "[A]" sobre o player so para gatilhos INTERATIVOS (tarefa). */
    if (g && g->kind == AREA_TAREFA_VERDE) {
        lv_obj_set_pos(s_prompt, s_px + 8, s_py - 17);
        lv_obj_remove_flag(s_prompt, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(s_prompt, LV_OBJ_FLAG_HIDDEN);
    }

    /* Botao A: interage com tarefa verde. */
    if (ui_btn_edge(BTN_A, &s_a_cache) && g && g->kind == AREA_TAREFA_VERDE) {
        s_tarefa_open = !s_tarefa_open;
        if (s_tarefa_open) {
            lv_obj_remove_flag(s_lbl_tarefa, LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_add_flag(s_lbl_tarefa, LV_OBJ_FLAG_HIDDEN);
        }
        ESP_LOGI(TAG, "tarefa verde: %s", s_tarefa_open ? "aberta" : "fechada");
    }

    /* B fecha o label de tarefa se estiver aberto. */
    if (ui_btn_edge(BTN_B, &s_b_cache) && s_tarefa_open) {
        s_tarefa_open = false;
        lv_obj_add_flag(s_lbl_tarefa, LV_OBJ_FLAG_HIDDEN);
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

/* Remove scrollabilidade — defensivo contra crash em readjust_scroll. */
static void no_scroll(lv_obj_t *o)
{
    lv_obj_remove_flag(o, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scroll_dir(o, LV_DIR_NONE);
}

static lv_obj_t *layer_full(lv_obj_t *parent, const lv_image_dsc_t *src,
                            int16_t x, int16_t y)
{
    lv_obj_t *img = lv_image_create(parent);
    lv_image_set_src(img, src);
    lv_obj_set_pos(img, x, y);
    no_scroll(img);
    return img;
}

static void free_all_assets(void)
{
    for (int i = 0; i < A_COUNT; ++i) {
        asset_loader_free(&s_assets[i]);
    }
}

/* Carrega da NAND todos os assets da tela. Em falha, desfaz os que ja
 * subiram e retorna false. */
static bool load_all_assets(void)
{
    for (int i = 0; i < A_COUNT; ++i) {
        const esp_err_t e = asset_loader_load(ASSET_TYPE_SPRITE,
                                              EMP_ASSET_ID[i], &s_assets[i]);
        if (e != ESP_OK) {
            ESP_LOGE(TAG, "asset_loader_load slot %d (id %u) falhou: %s",
                     i, EMP_ASSET_ID[i], esp_err_to_name(e));
            free_all_assets();
            return false;
        }
    }
    return true;
}

void screen_empresa_build(void)
{
    if (!load_all_assets()) {
        ESP_LOGE(TAG, "build abortado — assets da NAND indisponiveis "
                      "(rodou o upload via recovery?)");
        return;
    }

    s_root = lv_obj_create(lv_screen_active());
    lv_obj_set_size(s_root, 480, 320);
    lv_obj_set_pos(s_root, 0, 0);
    lv_obj_set_style_bg_color(s_root, lv_color_black(), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(s_root, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(s_root, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(s_root, 0, LV_PART_MAIN);
    no_scroll(s_root);

    layer_full(s_root, &s_assets[A_PISO].dsc, 0, 0);
    layer_full(s_root, &s_assets[A_PAREDES].dsc, 0, 0);

    s_player = lv_image_create(s_root);
    lv_image_set_src(s_player, &s_assets[A_PLAYER].dsc);
    lv_obj_set_size(s_player, PFRAME_W, PFRAME_H);
    lv_image_set_inner_align(s_player, LV_IMAGE_ALIGN_TOP_LEFT);
    no_scroll(s_player);
    apply_spawn_from_table();
    s_dir = 2; s_walk_idx = 1; s_walk_ms = 0;   /* RIGHT */
    s_tarefa_open = false;
    s_porta_armed = false;   /* spawn pode ser perto da porta — rearma ao sair */
    s_a_cache = button_hal_peek(BTN_A);
    s_b_cache = button_hal_peek(BTN_B);
    lv_obj_set_pos(s_player, s_px, s_py);
    apply_player_frame();

    layer_full(s_root, &s_assets[A_COMPLEMENTO].dsc,
               s_assets[A_COMPLEMENTO].off_x, s_assets[A_COMPLEMENTO].off_y);

    /* NPC TI — default pose 2 (PARA_CIMA = de costas, trabalhando) */
    s_npc = lv_image_create(s_root);
    no_scroll(s_npc);
    s_npc_pose = 255;  /* forca set_npc_pose() executar na primeira chamada */
    set_npc_pose(2);

    /* Icones de tarefa (amarela: NPC TI; verde: PC de tarefa) */
    s_icone_am = lv_image_create(s_root);
    lv_image_set_src(s_icone_am, &s_assets[A_ICONE_AM].dsc);
    lv_obj_set_pos(s_icone_am, s_assets[A_ICONE_AM].off_x,
                               s_assets[A_ICONE_AM].off_y);
    no_scroll(s_icone_am);

    s_icone_vd = lv_image_create(s_root);
    lv_image_set_src(s_icone_vd, &s_assets[A_ICONE_VD].dsc);
    lv_obj_set_pos(s_icone_vd, s_assets[A_ICONE_VD].off_x,
                               s_assets[A_ICONE_VD].off_y);
    no_scroll(s_icone_vd);

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
    no_scroll(s_lbl_tarefa);
    lv_obj_add_flag(s_lbl_tarefa, LV_OBJ_FLAG_HIDDEN);

    /* Prompt "[A]" — segue o player perto de gatilho. Oculto por padrao. */
    s_prompt = lv_label_create(s_root);
    lv_label_set_text(s_prompt, "[A]");
    lv_obj_set_style_text_color(s_prompt, lv_color_hex(0xFFD000), LV_PART_MAIN);
    lv_obj_set_style_bg_color(s_prompt, lv_color_black(), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(s_prompt, LV_OPA_70, LV_PART_MAIN);
    lv_obj_set_style_pad_all(s_prompt, 2, LV_PART_MAIN);
    no_scroll(s_prompt);
    lv_obj_add_flag(s_prompt, LV_OBJ_FLAG_HIDDEN);

    s_timer = lv_timer_create(empresa_tick, UI_TICK_MS, NULL);
    ESP_LOGI(TAG, "empresa built (player @ %d,%d)", s_px, s_py);
}

void screen_empresa_destroy(void)
{
    if (s_timer) { lv_timer_delete(s_timer); s_timer = NULL; }
    if (s_root)  {
        lv_obj_delete(s_root);
        s_root = NULL;
        s_player = s_npc = s_icone_am = s_icone_vd = NULL;
        s_prompt = s_lbl_tarefa = NULL;
    }
    /* Libera os pixels da PSRAM DEPOIS de deletar os objetos LVGL que
     * apontavam para eles. */
    free_all_assets();
}
