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
#include "gamestate.h"
#include "room_layout_loader.h"
#include "entity_pool.h"
#include "entity.h"
#include "y_sort.h"
#include "screen_tarefa_verde.h"
#include "screen_tarefa_amarela.h"
#include "screen_servidor_menu.h"
#include "screen_web_setor.h"
#include "engine.h"
#include "threat.h"
#include "asset_icone_vermelho.h"

static const char *TAG = "UI_EMPRESA";

/* s_px/s_py: top-left do lv_obj do player (lv coords, nao pivot).
 * Spawn de SPOWN_ENTRADA_ESCRITORIO em INTERACOES.txt: pivot(24,165)
 * → lv_pos(24-16, 165-48) = (8, 117). Posicao inicial antes do JSON ser lido. */
static int16_t s_px = 8;
static int16_t s_py = 117;
static room_player_anim_t s_anim = { .dir = 2, .walk_idx = 1, .walk_ms = 0 };

static lv_obj_t   *s_root          = NULL;
static lv_obj_t   *s_game_layer    = NULL;   /* piso + entidades; Y-sort opera aqui */
static lv_obj_t   *s_ui_layer      = NULL;   /* overlays UI — sempre na frente */
static lv_obj_t   *s_player        = NULL;
static entity_t   *s_player_entity = NULL;   /* referencia para atualizar sort_y */
static lv_obj_t   *s_npc           = NULL;   /* NPC_02 (interativo) */
/* Ícones por servidor: [0]=A(esquerda), [1]=B(direita) */
static lv_obj_t   *s_icone_am[2] = { NULL, NULL };
static lv_obj_t   *s_icone_vm[2] = { NULL, NULL };
static lv_obj_t   *s_icone_vd    = NULL;
static lv_obj_t   *s_prompt      = NULL;
static lv_timer_t *s_timer       = NULL;
static bool        s_npc_facing  = false;
static web_setor_id_t s_current_srv = WEB_SETOR_ESQUERDA;

static button_state_t s_a_cache         = BTN_RELEASED;
static uint8_t        s_tarefa_am_srv   = 0;
/* Contador de ticks para blink dos ícones (compartilhado). */
static uint32_t       s_blink_ticks     = 0;

static lv_obj_t      *s_srv_lost_overlay = NULL;
static lv_timer_t    *s_srv_lost_timer   = NULL;
static button_state_t s_b_lost_cache     = BTN_RELEASED;

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
/* Ícone vermelho vem de flash (asset_icone_vermelho.h) — não usa slot SD. */

/*
 * Deslocamento X entre servidor A (pivot 336,95) e servidor B (pivot 447,95).
 * Usado para posicionar os ícones do servidor B relativos ao A.
 */
#define SRV_B_OFFSET_X  111
static loaded_asset_t s_assets[A_COUNT];

static const room_player_box_t s_player_box = {
    .off_x = 8, .off_y = 36, .w = 16, .h = 12,
};

static room_collision_t s_room_col;

static void on_tarefa_vd_done(tarefa_vd_result_t result)
{
    ESP_LOGI(TAG, "tarefa verde: %s",
             result == TAREFA_VD_CONCLUIDA ? "concluida" : "cancelada");
    if (result == TAREFA_VD_CONCLUIDA) {
        gamestate_concluir_verde();
        if (s_icone_vd) lv_obj_add_flag(s_icone_vd, LV_OBJ_FLAG_HIDDEN);
    }
}

static void on_tarefa_am_done(tarefa_am_result_t result)
{
    ESP_LOGI(TAG, "tarefa amarela srv%u: %s", (unsigned)s_tarefa_am_srv,
             result == TAREFA_AM_CONCLUIDA ? "concluida" : "cancelada");
    /* gamestate_concluir_amarela ja e chamado dentro de screen_tarefa_amarela. */
}

static void on_servidor_menu_done(servidor_menu_result_t result)
{
    if (result == SERVIDOR_MENU_BACKUP) {
        s_tarefa_am_srv = (uint8_t)s_current_srv;
        screen_tarefa_amarela_build(s_tarefa_am_srv, on_tarefa_am_done);
    } else if (result == SERVIDOR_MENU_WEB) {
        screen_web_setor_build(s_current_srv);
    }
    /* CANCELADO: retoma exploracao sem acao */
}

static void srv_lost_tick(lv_timer_t *t)
{
    (void)t;
    if (!s_srv_lost_overlay) return;
    if (fsm_get_state() == GAME_STATE_PAUSE) return;   /* sala viva sob o pause */
    if (ui_btn_edge(BTN_B, &s_b_lost_cache)) {
        lv_timer_delete(s_srv_lost_timer);
        s_srv_lost_timer = NULL;
        lv_obj_delete(s_srv_lost_overlay);
        s_srv_lost_overlay = NULL;
    }
}

static void show_srv_lost(uint8_t srv_id)
{
    if (s_srv_lost_overlay) return;

    s_srv_lost_overlay = lv_obj_create(lv_screen_active());
    lv_obj_set_size(s_srv_lost_overlay, 480, 320);
    lv_obj_set_pos(s_srv_lost_overlay, 0, 0);
    lv_obj_set_style_bg_color(s_srv_lost_overlay, lv_color_hex(0x1A0000), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(s_srv_lost_overlay, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(s_srv_lost_overlay, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(s_srv_lost_overlay, 0, LV_PART_MAIN);
    lv_obj_remove_flag(s_srv_lost_overlay, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_radius(s_srv_lost_overlay, 0, LV_PART_MAIN);

    lv_obj_t *lbl_titulo = lv_label_create(s_srv_lost_overlay);
    lv_label_set_text_fmt(lbl_titulo, "[ SERVIDOR %c INUTILIZADO ]", srv_id == 0 ? 'A' : 'B');
    lv_obj_set_style_text_color(lbl_titulo, lv_color_hex(0xFF3333), LV_PART_MAIN);
    lv_obj_set_pos(lbl_titulo, 100, 110);

    lv_obj_t *lbl_causa = lv_label_create(s_srv_lost_overlay);
    lv_label_set_text_fmt(lbl_causa, "Causa: %s", engine_server_lost_nome(srv_id));
    lv_obj_set_style_text_color(lbl_causa, lv_color_hex(0xFF9999), LV_PART_MAIN);
    lv_obj_set_pos(lbl_causa, 170, 155);

    lv_obj_t *lbl_hint = lv_label_create(s_srv_lost_overlay);
    lv_label_set_text(lbl_hint, "[B] Fechar");
    lv_obj_set_style_text_color(lbl_hint, lv_color_hex(0x666666), LV_PART_MAIN);
    lv_obj_set_pos(lbl_hint, 205, 215);

    s_b_lost_cache   = button_hal_peek(BTN_B);
    s_srv_lost_timer = lv_timer_create(srv_lost_tick, UI_TICK_MS, NULL);
    ESP_LOGI(TAG, "servidor %u perdido: overlay aberto", (unsigned)srv_id);
}

static void empresa_tick(lv_timer_t *t)
{
    (void)t;
    if (!s_root || !s_player) return;
    if (fsm_get_state() == GAME_STATE_PAUSE) return;

    screen_hud_tick();

    s_blink_ticks++;

    /* Ícone verde: pisca devagar quando disponível, some quando concluída. */
    if (s_icone_vd) {
        const tarefa_estado_t vest = gamestate_verde_estado();
        if (vest == TAREFA_CONCLUIDA) {
            lv_obj_add_flag(s_icone_vd, LV_OBJ_FLAG_HIDDEN);
        } else if (vest == TAREFA_DISPONIVEL) {
            /* 600ms period: 4 ticks ON, 2 ticks OFF */
            const bool vis = (s_blink_ticks % 6) < 4;
            if (vis) lv_obj_remove_flag(s_icone_vd, LV_OBJ_FLAG_HIDDEN);
            else     lv_obj_add_flag   (s_icone_vd, LV_OBJ_FLAG_HIDDEN);
        }
    }

    /* Ícones amarelo/vermelho: independentes por servidor. */
    for (uint8_t srv = 0; srv < 2; srv++) {
        const bool has_am = (gamestate_amarela_estado(srv) == TAREFA_DISPONIVEL);
        const bool has_vm = threat_get_active(srv, NULL);

        lv_obj_t *obj_am = s_icone_am[srv];
        lv_obj_t *obj_vm = s_icone_vm[srv];

        if (!obj_am && !obj_vm) continue;

        if (!has_am && !has_vm) {
            /* Nenhum alerta — oculta tudo. */
            if (obj_am) lv_obj_add_flag(obj_am, LV_OBJ_FLAG_HIDDEN);
            if (obj_vm) lv_obj_add_flag(obj_vm, LV_OBJ_FLAG_HIDDEN);
        } else if (has_am && !has_vm) {
            /* Só amarelo: pisca devagar (600ms period: 4 ON / 2 OFF). */
            const bool vis = (s_blink_ticks % 6) < 4;
            if (obj_am) {
                if (vis) lv_obj_remove_flag(obj_am, LV_OBJ_FLAG_HIDDEN);
                else     lv_obj_add_flag   (obj_am, LV_OBJ_FLAG_HIDDEN);
            }
            if (obj_vm) lv_obj_add_flag(obj_vm, LV_OBJ_FLAG_HIDDEN);
        } else if (!has_am && has_vm) {
            /* Só vermelho: pisca rápido (300ms period: 2 ON / 1 OFF). */
            const bool vis = (s_blink_ticks % 3) < 2;
            if (obj_am) lv_obj_add_flag   (obj_am, LV_OBJ_FLAG_HIDDEN);
            if (obj_vm) {
                if (vis) lv_obj_remove_flag(obj_vm, LV_OBJ_FLAG_HIDDEN);
                else     lv_obj_add_flag   (obj_vm, LV_OBJ_FLAG_HIDDEN);
            }
        } else {
            /* Ambos: alterna entre amarelo e vermelho a cada 500ms (5 ticks). */
            const bool show_am = (s_blink_ticks % 10) < 5;
            if (obj_am) {
                if (show_am) lv_obj_remove_flag(obj_am, LV_OBJ_FLAG_HIDDEN);
                else         lv_obj_add_flag   (obj_am, LV_OBJ_FLAG_HIDDEN);
            }
            if (obj_vm) {
                if (!show_am) lv_obj_remove_flag(obj_vm, LV_OBJ_FLAG_HIDDEN);
                else          lv_obj_add_flag   (obj_vm, LV_OBJ_FLAG_HIDDEN);
            }
        }
    }

    if (screen_tarefa_verde_is_open()   || screen_tarefa_amarela_is_open() ||
        screen_servidor_menu_is_open() ||
        screen_web_setor_is_open(WEB_SETOR_ESQUERDA) ||
        screen_web_setor_is_open(WEB_SETOR_DIREITA)  ||
        s_srv_lost_overlay != NULL) return;

    /* Sub-FSM NFC ativa (terminal/waiting/lock/deploy): bloqueia movimento.
     * A FSM controla esses sub-estados via queue de botões. */
    if (fsm_get_gameplay_substate() != GAMEPLAY_SUB_EXPLORANDO) return;

    const joystick_data_t j = joystick_hal_get_state();
    const int jx = j.x;
    const int jy = j.y;
    const int jx_mag = (jx < 0 ? -jx : jx) * (int)JOY_X_BOOST_PCT / 100;
    const int sx_mag = room_speed_from_mag(jx_mag > 100 ? 100 : jx_mag);
    const int sy_mag = room_speed_from_mag(jy < 0 ? -jy : jy);
    const int dx = (sx_mag == 0) ? 0 : (jx > 0 ? +1 : -1);
    const int dy = (sy_mag == 0) ? 0 : (jy > 0 ? +1 : -1);

    room_anim_update_dir(&s_anim, jx, jy);

    /* +50% de velocidade durante ataque vermelho ativo — cria urgencia. */
    const int speed_pct = fsm_get_attack_active() ? 150 : 100;
    if (dx != 0) {
        const int nx = s_px + dx * sx_mag * speed_pct / 100;
        if (!room_collides_at(&s_room_col, &s_player_box, nx, s_py)) s_px = nx;
    }
    if (dy != 0) {
        const int ny = s_py + dy * sy_mag * speed_pct / 100;
        if (!room_collides_at(&s_room_col, &s_player_box, s_px, ny)) s_py = ny;
    }

    lv_obj_set_pos(s_player, s_px, s_py);
    if (s_player_entity) {
        s_player_entity->x = (int16_t)(s_px + PLAYER_FRAME_W / 2);
        s_player_entity->y = (int16_t)(s_py + PLAYER_FRAME_H);
        y_sort_mark_dirty();
    }
    room_anim_step(&s_anim, s_player, dx, dy, UI_TICK_MS, PLAYER_FRAME_W, PLAYER_FRAME_H);

    const collision_rect_t *g = room_gatilho_at(&s_room_col, &s_player_box, s_px, s_py);

    const bool near_vd    = (g && g->kind == AREA_TAREFA_VERDE);
    const bool near_am    = (g && g->kind == AREA_TAREFA_AMARELA);
    const bool near_srv_a = (g && g->kind == AREA_SERVIDOR);
    const bool near_srv_b = (g && g->kind == AREA_SERVIDOR_B);
    const bool near_srv   = near_srv_a || near_srv_b;

    fsm_set_player_at_equipment(near_vd || near_am || near_srv);

    /* NPC_02 troca de frame por proximidade. */
    const bool near_npc = (g && g->kind == AREA_INTERACAO_NPC);
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

    if (near_vd || near_am || near_srv) {
        lv_obj_set_pos(s_prompt, s_px + 8, s_py - 17);
        lv_obj_remove_flag(s_prompt, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(s_prompt, LV_OBJ_FLAG_HIDDEN);
    }

    if (ui_btn_edge(BTN_A, &s_a_cache)) {
        if (near_vd) {
            fsm_set_player_at_equipment(false);
            screen_tarefa_verde_build(on_tarefa_vd_done);
        } else if (near_srv) {
            fsm_set_player_at_equipment(false);
            const uint8_t srv_id = near_srv_b ? 1u : 0u;
            if (engine_server_is_lost(srv_id)) {
                show_srv_lost(srv_id);
            } else {
                s_current_srv = near_srv_b ? WEB_SETOR_DIREITA : WEB_SETOR_ESQUERDA;
                screen_servidor_menu_build(on_servidor_menu_done);
            }
        }
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

    s_game_layer = lv_obj_create(s_root);
    lv_obj_set_size(s_game_layer, 480, 320);
    lv_obj_set_pos(s_game_layer, 0, 0);
    lv_obj_set_style_bg_opa(s_game_layer, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(s_game_layer, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(s_game_layer, 0, LV_PART_MAIN);
    no_scroll(s_game_layer);

    s_ui_layer = lv_obj_create(s_root);
    lv_obj_set_size(s_ui_layer, 480, 320);
    lv_obj_set_pos(s_ui_layer, 0, 0);
    lv_obj_set_style_bg_opa(s_ui_layer, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(s_ui_layer, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(s_ui_layer, 0, LV_PART_MAIN);
    no_scroll(s_ui_layer);

    /* L0 — piso background */
    layer_full(s_game_layer, &s_assets[A_PISO].dsc, 0, 0);

    /* Instancia todos os sprites individuais + NPCs + player via JSON de layout. */
    entity_t *player_entity = NULL;
    room_layout_spawn(s_game_layer, "empresa", &player_entity);

    /* Posicao do player — spawn de SPOWN_ENTRADA_ESCRITORIO (pivot 24,165). */
    if (player_entity != NULL && player_entity->lv_obj != NULL) {
        s_player        = player_entity->lv_obj;
        s_player_entity = player_entity;
        s_px = player_entity->x - PLAYER_FRAME_W / 2;
        s_py = player_entity->y - PLAYER_FRAME_H;
    } else {
        s_player        = lv_image_create(s_game_layer);
        s_player_entity = NULL;
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
    s_a_cache = button_hal_peek(BTN_A);
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

    /* Ícones de tarefa — iniciam HIDDEN; tick controla visibilidade e blink.
     * off_x/off_y do amarelo A dão a posição sobre o servidor A no canvas.
     * Servidor B fica SRV_B_OFFSET_X pixels à direita (mesmo Y). */
    const int16_t am_ax = s_assets[A_ICONE_AM].off_x;
    const int16_t am_ay = s_assets[A_ICONE_AM].off_y;

    /* Amarelo servidor A */
    s_icone_am[0] = lv_image_create(s_ui_layer);
    lv_image_set_src(s_icone_am[0], &s_assets[A_ICONE_AM].dsc);
    lv_obj_set_pos(s_icone_am[0], am_ax, am_ay);
    lv_obj_set_style_radius(s_icone_am[0], 0, LV_PART_MAIN);
    no_scroll(s_icone_am[0]);
    lv_obj_add_flag(s_icone_am[0], LV_OBJ_FLAG_HIDDEN);

    /* Amarelo servidor B */
    s_icone_am[1] = lv_image_create(s_ui_layer);
    lv_image_set_src(s_icone_am[1], &s_assets[A_ICONE_AM].dsc);
    lv_obj_set_pos(s_icone_am[1], am_ax + SRV_B_OFFSET_X, am_ay);
    lv_obj_set_style_radius(s_icone_am[1], 0, LV_PART_MAIN);
    no_scroll(s_icone_am[1]);
    lv_obj_add_flag(s_icone_am[1], LV_OBJ_FLAG_HIDDEN);

    /* Vermelho vem de flash (16x16 RGB565A8) — mesma posição Y do amarelo. */
    const lv_image_dsc_t *vm_dsc = asset_icone_vermelho_get_dsc();

    /* Vermelho servidor A */
    s_icone_vm[0] = lv_image_create(s_ui_layer);
    lv_image_set_src(s_icone_vm[0], vm_dsc);
    lv_obj_set_pos(s_icone_vm[0], am_ax, am_ay);
    lv_obj_set_style_radius(s_icone_vm[0], 0, LV_PART_MAIN);
    no_scroll(s_icone_vm[0]);
    lv_obj_add_flag(s_icone_vm[0], LV_OBJ_FLAG_HIDDEN);

    /* Vermelho servidor B */
    s_icone_vm[1] = lv_image_create(s_ui_layer);
    lv_image_set_src(s_icone_vm[1], vm_dsc);
    lv_obj_set_pos(s_icone_vm[1], am_ax + SRV_B_OFFSET_X, am_ay);
    lv_obj_set_style_radius(s_icone_vm[1], 0, LV_PART_MAIN);
    no_scroll(s_icone_vm[1]);
    lv_obj_add_flag(s_icone_vm[1], LV_OBJ_FLAG_HIDDEN);

    s_icone_vd = lv_image_create(s_ui_layer);
    lv_image_set_src(s_icone_vd, &s_assets[A_ICONE_VD].dsc);
    lv_obj_set_pos(s_icone_vd, s_assets[A_ICONE_VD].off_x, s_assets[A_ICONE_VD].off_y);
    lv_obj_set_style_radius(s_icone_vd, 0, LV_PART_MAIN);
    no_scroll(s_icone_vd);
    lv_obj_add_flag(s_icone_vd, LV_OBJ_FLAG_HIDDEN);

    s_prompt = lv_label_create(s_ui_layer);
    lv_label_set_text(s_prompt, "[A]");
    lv_obj_set_style_text_color(s_prompt, lv_color_hex(0xFFD000), LV_PART_MAIN);
    lv_obj_set_style_bg_color(s_prompt, lv_color_black(), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(s_prompt, LV_OPA_70, LV_PART_MAIN);
    lv_obj_set_style_pad_all(s_prompt, 2, LV_PART_MAIN);
    no_scroll(s_prompt);
    lv_obj_add_flag(s_prompt, LV_OBJ_FLAG_HIDDEN);

    gamestate_iniciar_expediente();  /* 1a entrada no escritorio = inicia expediente */
    screen_hud_build(s_ui_layer);

    s_timer = lv_timer_create(empresa_tick, UI_TICK_MS, NULL);
    ESP_LOGI(TAG, "empresa built (player @ %d,%d)", s_px, s_py);
}

void screen_empresa_destroy(void)
{
    /* Overlays de gameplay sao filhos da screen LVGL (nao do s_root): fechar
     * aqui, senao sobrevivem a troca de tela com is_open()==true e travam o
     * tick da proxima empresa (soft-lock invisivel). Todos idempotentes. */
    screen_web_setor_destroy(WEB_SETOR_ESQUERDA);
    screen_web_setor_destroy(WEB_SETOR_DIREITA);
    screen_tarefa_amarela_destroy();
    screen_tarefa_verde_destroy();
    screen_servidor_menu_destroy();

    if (s_srv_lost_timer)   { lv_timer_delete(s_srv_lost_timer);   s_srv_lost_timer   = NULL; }
    if (s_srv_lost_overlay) { lv_obj_delete(s_srv_lost_overlay);   s_srv_lost_overlay = NULL; }
    if (s_timer) { lv_timer_delete(s_timer); s_timer = NULL; }
    screen_hud_destroy();
    entity_pool_clear();
    s_player        = NULL;
    s_player_entity = NULL;
    s_npc           = NULL;
    if (s_root) {
        lv_obj_delete(s_root);
        s_root = NULL;
        s_game_layer = s_ui_layer = NULL;
        s_icone_am[0] = s_icone_am[1] = NULL;
        s_icone_vm[0] = s_icone_vm[1] = NULL;
        s_icone_vd = s_prompt = NULL;
        s_blink_ticks = 0;
    }
    free_all_assets();
}
