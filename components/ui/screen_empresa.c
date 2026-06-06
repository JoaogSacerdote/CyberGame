#include "ui.h"
#include "ui_internal.h"

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
#include "screen_room.h"
#include "game_config.h"
#include "room_layout_loader.h"
#include "entity_pool.h"
#include "entity.h"

static const char *TAG = "UI_EMPRESA";

/* s_px/s_py: top-left do lv_obj do player (lv coords, nao pivot).
 * Spawn de SPOWN_ENTRADA_ESCRITORIO em INTERACOES.txt: pivot(12,160)
 * → lv_pos(12-16, 160-48) = (-4, 112). Ajustado para ficar dentro da tela. */
static int16_t s_px = 0;
static int16_t s_py = 112;
static room_player_anim_t s_anim = { .dir = 2, .walk_idx = 1, .walk_ms = 0 };

static lv_obj_t   *s_root        = NULL;
static lv_obj_t   *s_player      = NULL;
static lv_obj_t   *s_npc         = NULL;   /* NPC_02 (interativo) */
static lv_obj_t   *s_icone_am    = NULL;
static lv_obj_t   *s_icone_vd    = NULL;
static lv_obj_t   *s_prompt      = NULL;
static lv_obj_t   *s_lbl_tarefa  = NULL;
static lv_timer_t *s_timer       = NULL;
static bool        s_tarefa_open = false;
static bool        s_npc_facing  = false;

static button_state_t s_a_cache = BTN_RELEASED;
static button_state_t s_b_cache = BTN_RELEASED;

/* === Assets UI: piso, player sheet, frames NPC_02, icones === */
typedef enum {
    A_PISO = 0,
    A_PLAYER,
    A_NPC_02_IDLE,
    A_NPC_02_DIALOG,
    A_ICONE_AM,
    A_ICONE_VD,
    A_COUNT
} emp_slot_t;

static const uint16_t EMP_ASSET_ID[A_COUNT] = {
    [A_PISO]         = ASSET_EMP_PISO,
    [A_PLAYER]       = ASSET_PLAYER,
    [A_NPC_02_IDLE]  = ASSET_EMP_NPC_02_IDLE,
    [A_NPC_02_DIALOG]= ASSET_EMP_NPC_02_DIALOG,
    [A_ICONE_AM]     = ASSET_EMP_ICONE_AMARELO,
    [A_ICONE_VD]     = ASSET_EMP_ICONE_VERDE,
};
static loaded_asset_t s_assets[A_COUNT];

static const room_player_box_t s_player_box = {
    .off_x = 8, .off_y = 36, .w = 16, .h = 12,
};

static room_collision_t s_room_col;

static void empresa_tick(lv_timer_t *t)
{
    (void)t;
    if (!s_root || !s_player) return;
    if (fsm_get_state() == GAME_STATE_PAUSE) return;

    screen_hud_tick();

    const joystick_data_t j = joystick_hal_get_state();
    const int jx = j.x;
    const int jy = j.y;
    const int sx_mag = room_speed_from_mag(jx < 0 ? -jx : jx);
    const int sy_mag = room_speed_from_mag(jy < 0 ? -jy : jy);
    const int dx = (sx_mag == 0) ? 0 : (jx > 0 ? +1 : -1);
    const int dy = (sy_mag == 0) ? 0 : (jy > 0 ? +1 : -1);

    room_anim_update_dir(&s_anim, jx, jy);

    if (dx != 0) {
        const int nx = s_px + dx * sx_mag;
        if (!room_collides_at(&s_room_col, &s_player_box, nx, s_py)) s_px = nx;
    }
    if (dy != 0) {
        const int ny = s_py + dy * sy_mag;
        if (!room_collides_at(&s_room_col, &s_player_box, s_px, ny)) s_py = ny;
    }

    lv_obj_set_pos(s_player, s_px, s_py);
    room_anim_step(&s_anim, s_player, dx, dy, UI_TICK_MS, PLAYER_FRAME_W, PLAYER_FRAME_H);

    const collision_rect_t *g = room_gatilho_at(&s_room_col, &s_player_box, s_px, s_py);

    fsm_set_player_at_equipment(g && g->kind == AREA_TAREFA_VERDE);

    /* NPC_02 troca de frame por proximidade. */
    const bool near_npc = (g && g->kind == AREA_INTERACAO_NPC_TI_BAIXO);
    if (near_npc != s_npc_facing) {
        s_npc_facing = near_npc;
        if (s_npc) {
            lv_image_set_src(s_npc, near_npc
                             ? &s_assets[A_NPC_02_DIALOG].dsc
                             : &s_assets[A_NPC_02_IDLE].dsc);
        }
    }

    if (g && g->kind == AREA_PORTA_RECEPCAO) {
        ESP_LOGI(TAG, "porta recepcao (contato) -> trocando sala");
        fsm_set_gameplay_sala(GAMEPLAY_SALA_RECEPCAO);
        return;
    }

    if (g && g->kind == AREA_TAREFA_VERDE) {
        lv_obj_set_pos(s_prompt, s_px + 8, s_py - 17);
        lv_obj_remove_flag(s_prompt, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(s_prompt, LV_OBJ_FLAG_HIDDEN);
    }

    if (ui_btn_edge(BTN_A, &s_a_cache) && g && g->kind == AREA_TAREFA_VERDE) {
        s_tarefa_open = !s_tarefa_open;
        if (s_tarefa_open) {
            lv_obj_remove_flag(s_lbl_tarefa, LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_add_flag(s_lbl_tarefa, LV_OBJ_FLAG_HIDDEN);
        }
    }

    if (ui_btn_edge(BTN_B, &s_b_cache) && s_tarefa_open) {
        s_tarefa_open = false;
        lv_obj_add_flag(s_lbl_tarefa, LV_OBJ_FLAG_HIDDEN);
    }
}

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
    s_room_col.obstaculos       = collision_empresa_obstaculos;
    s_room_col.obstaculos_count = collision_empresa_obstaculos_count;
    s_room_col.gatilhos         = collision_empresa_gatilhos;
    s_room_col.gatilhos_count   = collision_empresa_gatilhos_count;
    s_room_col.screen_w         = 480;
    s_room_col.screen_h         = 320;

    if (!load_all_assets()) {
        ESP_LOGE(TAG, "build abortado — assets do cartao SD indisponiveis");
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

    /* L0 — piso background */
    layer_full(s_root, &s_assets[A_PISO].dsc, 0, 0);

    /* Instancia todos os sprites individuais + NPCs + player via JSON de layout. */
    entity_t *player_entity = NULL;
    room_layout_spawn(s_root, "empresa", &player_entity);

    /* Posicao do player — spawn de SPOWN_ENTRADA_ESCRITORIO (pivot 12,160). */
    if (player_entity != NULL && player_entity->lv_obj != NULL) {
        s_player = player_entity->lv_obj;
        s_px = player_entity->x - PLAYER_FRAME_W / 2;
        s_py = player_entity->y - PLAYER_FRAME_H;
    } else {
        s_player = lv_image_create(s_root);
        lv_image_set_src(s_player, &s_assets[A_PLAYER].dsc);
        lv_obj_set_size(s_player, PLAYER_FRAME_W, PLAYER_FRAME_H);
        lv_image_set_inner_align(s_player, LV_IMAGE_ALIGN_TOP_LEFT);
        no_scroll(s_player);
        s_px = 0; s_py = 112;
    }

    {
        const collision_rect_t *g = room_gatilho_at(&s_room_col, &s_player_box, s_px, s_py);
        if (g && g->kind == AREA_PORTA_RECEPCAO) {
            ESP_LOGW(TAG, "spawn (%d,%d) caiu em gatilho de porta — risco de loop", s_px, s_py);
        }
    }
    s_anim.dir = 2; s_anim.walk_idx = 1; s_anim.walk_ms = 0;
    s_tarefa_open = false;
    s_a_cache = button_hal_peek(BTN_A);
    s_b_cache = button_hal_peek(BTN_B);
    lv_obj_set_pos(s_player, s_px, s_py);
    room_anim_step(&s_anim, s_player, 0, 0, 0, PLAYER_FRAME_W, PLAYER_FRAME_H);

    /* Encontra NPC_02 (interativo) no pool. */
    s_npc = NULL;
    s_npc_facing = false;
    for (size_t i = 0; i < entity_pool_count(); i++) {
        entity_t *e = entity_pool_at(i);
        if (e->type == ENTITY_TYPE_NPC) {
            s_npc = e->lv_obj;
            if (s_npc) lv_image_set_src(s_npc, &s_assets[A_NPC_02_IDLE].dsc);
            break;
        }
    }

    /* Icones de tarefa (posicao via crop offset — ajustar em jogo se necessario) */
    s_icone_am = lv_image_create(s_root);
    lv_image_set_src(s_icone_am, &s_assets[A_ICONE_AM].dsc);
    lv_obj_set_pos(s_icone_am, s_assets[A_ICONE_AM].off_x, s_assets[A_ICONE_AM].off_y);
    no_scroll(s_icone_am);

    s_icone_vd = lv_image_create(s_root);
    lv_image_set_src(s_icone_vd, &s_assets[A_ICONE_VD].dsc);
    lv_obj_set_pos(s_icone_vd, s_assets[A_ICONE_VD].off_x, s_assets[A_ICONE_VD].off_y);
    no_scroll(s_icone_vd);

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

    s_prompt = lv_label_create(s_root);
    lv_label_set_text(s_prompt, "[A]");
    lv_obj_set_style_text_color(s_prompt, lv_color_hex(0xFFD000), LV_PART_MAIN);
    lv_obj_set_style_bg_color(s_prompt, lv_color_black(), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(s_prompt, LV_OPA_70, LV_PART_MAIN);
    lv_obj_set_style_pad_all(s_prompt, 2, LV_PART_MAIN);
    no_scroll(s_prompt);
    lv_obj_add_flag(s_prompt, LV_OBJ_FLAG_HIDDEN);

    screen_hud_build(s_root);

    s_timer = lv_timer_create(empresa_tick, UI_TICK_MS, NULL);
    ESP_LOGI(TAG, "empresa built (player @ %d,%d)", s_px, s_py);
}

void screen_empresa_destroy(void)
{
    if (s_timer) { lv_timer_delete(s_timer); s_timer = NULL; }
    screen_hud_destroy();
    entity_pool_clear();
    s_player = NULL;
    s_npc    = NULL;
    if (s_root) {
        lv_obj_delete(s_root);
        s_root = NULL;
        s_icone_am = s_icone_vd = s_prompt = s_lbl_tarefa = NULL;
    }
    free_all_assets();
}
