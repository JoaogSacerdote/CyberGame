#include "ui.h"
#include "ui_internal.h"

#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include "esp_log.h"
#include "lvgl.h"
#include "asset_loader.h"
#include "asset_ids.h"
#include "dialog_loader.h"
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
#include "y_sort.h"

static const char *TAG = "UI_RECEPCAO";

/* === Dialogo do recepcionista ===
 * Carregado do SD card no build (assets/DIALOGO.txt -> blob).
 * Edicao do texto vive em arquivo .txt + regravar o SD, sem rebuild do firmware. */
static dialog_t s_dialogo;

typedef enum {
    DLG_INACTIVE = 0,
    DLG_TYPING,
    DLG_WAITING,    /* texto completo, esperando A pra avancar */
} dlg_state_t;

/* s_px/s_py: top-left do lv_obj do player (LVGL coords, nao pivot).
 * Calculados no build a partir do pivot da entidade do JSON de layout. */
static int16_t s_px = 224;
static int16_t s_py = 184;
static room_player_anim_t s_anim = { .dir = 0, .walk_idx = 1, .walk_ms = 0 };
static bool    s_npc_facing = false;
static bool    s_icon_visible = true;
static uint32_t s_icon_blink_ms = 0;

static lv_obj_t   *s_root          = NULL;
static lv_obj_t   *s_game_layer    = NULL;   /* piso + entidades; Y-sort opera aqui */
static lv_obj_t   *s_ui_layer      = NULL;   /* overlays UI — sempre na frente */
static lv_obj_t   *s_player        = NULL;
static entity_t   *s_player_entity = NULL;   /* referencia para atualizar sort_y */
static lv_obj_t   *s_npc           = NULL;
static lv_obj_t   *s_icone       = NULL;
static lv_obj_t   *s_prompt      = NULL;   /* "[A]" — aparece sobre o player perto de gatilho */
static lv_obj_t   *s_dlg_box     = NULL;   /* PNG caixa de dialogo (oculto por default) */
static lv_obj_t   *s_dlg_text    = NULL;   /* texto typewriter */
static lv_obj_t   *s_dlg_hint    = NULL;   /* "[A] >>  [B] Pular" */
static lv_timer_t *s_timer       = NULL;

/* === Assets da tela: piso, player sheet, frames NPC, overlays UI ===
 * Furniture e NPC sao instanciados pelo room_layout_spawn (cache hit — sem
 * double-load de PSRAM). Aqui so os assets necessarios ao tick da tela. */
typedef enum {
    A_PISO = 0,
    A_PLAYER,
    A_NPC_IDLE,
    A_NPC_DIALOG,
    A_ICONE,
    A_CAIXA_DIALOGO,
    A_CAIXA_TEXTO,
    A_COUNT
} rec_slot_t;

static const uint16_t REC_ASSET_ID[A_COUNT] = {
    [A_PISO]          = ASSET_REC_PISO,
    [A_PLAYER]        = ASSET_PLAYER,
    [A_NPC_IDLE]      = ASSET_REC_NPC_01_IDLE,
    [A_NPC_DIALOG]    = ASSET_REC_NPC_01_DIALOG,
    [A_ICONE]         = ASSET_REC_ICONE_NOTIF,
    [A_CAIXA_DIALOGO] = ASSET_REC_CAIXA_DIALOGO,
    [A_CAIXA_TEXTO]   = ASSET_REC_CAIXA_TEXTO,
};
static loaded_asset_t s_assets[A_COUNT];

/* Estado do dialogo */
static dlg_state_t s_dlg_state = DLG_INACTIVE;
static bool        s_dlg_played = false;   /* nao re-toca dialogo apos jogador ja ter visto */
static uint8_t     s_dlg_line   = 0;
static uint16_t    s_dlg_char   = 0;
static uint32_t    s_dlg_typewriter_ms = 0;
static button_state_t s_a_cache = BTN_RELEASED;
static button_state_t s_b_cache = BTN_RELEASED;

/* Player bbox para colisao: pes (16x12 na base) — o sprite e 32x48 mas o "pe"
 * que toca o chao e bem menor. Usamos 16x12 com offset (8, 36) no frame. */
static const room_player_box_t s_player_box = {
    .off_x = 8, .off_y = 36, .w = 16, .h = 12,
};

/* Dados de colisao da sala — preenchidos no build apontando para as tabelas
 * em collision_data.h. Como obstaculos_count e variavel const externa, nao da
 * pra inicializar como const literal aqui. */
static room_collision_t s_room_col;

static void dlg_show_box(bool show)
{
    if (show) {
        lv_obj_remove_flag(s_dlg_box, LV_OBJ_FLAG_HIDDEN);
        lv_obj_remove_flag(s_dlg_text, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(s_dlg_hint, LV_OBJ_FLAG_HIDDEN);  /* hint aparece so quando WAITING */
    } else {
        lv_obj_add_flag(s_dlg_box, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(s_dlg_text, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(s_dlg_hint, LV_OBJ_FLAG_HIDDEN);
    }
}

static void dlg_start(void)
{
    s_dlg_state = DLG_TYPING;
    s_dlg_line = 0;
    s_dlg_char = 0;
    s_dlg_typewriter_ms = 0;
    lv_label_set_text(s_dlg_text, "");
    dlg_show_box(true);
    ESP_LOGI(TAG, "dialogo iniciado");
}

static void dlg_complete_line(void)
{
    lv_label_set_text(s_dlg_text, s_dialogo.lines[s_dlg_line]);
    s_dlg_char = strlen(s_dialogo.lines[s_dlg_line]);
    s_dlg_state = DLG_WAITING;
    lv_obj_remove_flag(s_dlg_hint, LV_OBJ_FLAG_HIDDEN);
}

static void dlg_next_line(void)
{
    s_dlg_line++;
    if (s_dlg_line >= s_dialogo.num_lines) {
        s_dlg_state = DLG_INACTIVE;
        s_dlg_played = true;
        dlg_show_box(false);
        ESP_LOGI(TAG, "dialogo encerrado (fim natural)");
        return;
    }
    s_dlg_char = 0;
    s_dlg_typewriter_ms = 0;
    s_dlg_state = DLG_TYPING;
    lv_label_set_text(s_dlg_text, "");
    lv_obj_add_flag(s_dlg_hint, LV_OBJ_FLAG_HIDDEN);
}

static void dlg_skip_all(void)
{
    s_dlg_state = DLG_INACTIVE;
    s_dlg_played = true;
    dlg_show_box(false);
    ESP_LOGI(TAG, "dialogo encerrado (skip)");
}

static void dlg_tick(uint32_t dt_ms)
{
    /* Le botoes (edge detection). Bloqueia o joystick / movimento. */
    if (ui_btn_edge(BTN_A, &s_a_cache)) {
        if (s_dlg_state == DLG_TYPING) {
            dlg_complete_line();
        } else if (s_dlg_state == DLG_WAITING) {
            dlg_next_line();
        }
    }
    if (ui_btn_edge(BTN_B, &s_b_cache)) {
        dlg_skip_all();
        return;
    }

    if (s_dlg_state != DLG_TYPING) return;

    s_dlg_typewriter_ms += dt_ms;
    while (s_dlg_typewriter_ms >= DIALOG_TYPE_PERIOD_MS && s_dlg_state == DLG_TYPING) {
        s_dlg_typewriter_ms -= DIALOG_TYPE_PERIOD_MS;
        const char *full = s_dialogo.lines[s_dlg_line];
        const size_t total = strlen(full);
        if (s_dlg_char >= total) {
            s_dlg_state = DLG_WAITING;
            lv_obj_remove_flag(s_dlg_hint, LV_OBJ_FLAG_HIDDEN);
            break;
        }
        s_dlg_char++;
        lv_label_set_text_fmt(s_dlg_text, "%.*s", (int)s_dlg_char, full);
    }
}

static void recepcao_tick(lv_timer_t *t)
{
    (void)t;

    if (!s_root || !s_player) return;

    if (fsm_get_state() == GAME_STATE_PAUSE) return;

    screen_hud_tick();

    if (s_dlg_state != DLG_INACTIVE) {
        dlg_tick(UI_TICK_MS);
        return;
    }

    const joystick_data_t j = joystick_hal_get_state();
    const int jx = j.x;
    const int jy = j.y;
    const int jx_mag = (jx < 0 ? -jx : jx) * (int)JOY_X_BOOST_PCT / 100;
    const int sx_mag = room_speed_from_mag(jx_mag > 100 ? 100 : jx_mag);
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
    if (s_player_entity) {
        s_player_entity->x = (int16_t)(s_px + PLAYER_FRAME_W / 2);
        s_player_entity->y = (int16_t)(s_py + PLAYER_FRAME_H);
        y_sort_mark_dirty();
    }
    room_anim_step(&s_anim, s_player, dx, dy, UI_TICK_MS, PLAYER_FRAME_W, PLAYER_FRAME_H);

    const collision_rect_t *g = room_gatilho_at(&s_room_col, &s_player_box, s_px, s_py);

    /* NPC troca de frame por proximidade (feedback automatico). Sem mudar posicao
     * — a entidade foi posicionada pelo pivot no JSON e permanece fixa. */
    const bool near_npc = (g && g->kind == AREA_INTERACAO_NPC);
    if (near_npc != s_npc_facing) {
        s_npc_facing = near_npc;
        if (s_npc) {
            lv_image_set_src(s_npc, near_npc
                             ? &s_assets[A_NPC_DIALOG].dsc
                             : &s_assets[A_NPC_IDLE].dsc);
        }
    }

    if (g && g->kind == AREA_PORTA_EMPRESA) {
        ESP_LOGI(TAG, "porta empresa (contato) -> trocando sala");
        fsm_set_gameplay_sala(GAMEPLAY_SALA_EMPRESA);
        return;
    }

    if (g && !room_is_porta(g->kind)) {
        lv_obj_set_pos(s_prompt, s_px + 8, s_py - 17);
        lv_obj_remove_flag(s_prompt, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(s_prompt, LV_OBJ_FLAG_HIDDEN);
    }

    if (s_icon_visible) {
        s_icon_blink_ms += UI_TICK_MS;
        if (s_icon_blink_ms >= 500) {
            s_icon_blink_ms = 0;
            if (lv_obj_has_flag(s_icone, LV_OBJ_FLAG_HIDDEN)) {
                lv_obj_remove_flag(s_icone, LV_OBJ_FLAG_HIDDEN);
            } else {
                lv_obj_add_flag(s_icone, LV_OBJ_FLAG_HIDDEN);
            }
        }
    }

    if (ui_btn_edge(BTN_A, &s_a_cache) && g && g->kind == AREA_INTERACAO_NPC) {
        if (!s_dlg_played) {
            s_icon_visible = false;
            lv_obj_add_flag(s_icone, LV_OBJ_FLAG_HIDDEN);
            s_b_cache = button_hal_peek(BTN_B);
            dlg_start();
            return;
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
                                              REC_ASSET_ID[i], &s_assets[i]);
        if (e != ESP_OK) {
            ESP_LOGE(TAG, "asset_loader_load slot %d (id %u) falhou: %s",
                     i, REC_ASSET_ID[i], esp_err_to_name(e));
            free_all_assets();
            return false;
        }
    }
    const esp_err_t derr = dialog_loader_load(ASSET_REC_DIALOG, &s_dialogo);
    if (derr != ESP_OK) {
        ESP_LOGE(TAG, "dialog_loader_load falhou: %s", esp_err_to_name(derr));
        free_all_assets();
        return false;
    }
    return true;
}

void screen_recepcao_build(void)
{
    s_room_col.obstaculos       = collision_recepcao_obstaculos;
    s_room_col.obstaculos_count = collision_recepcao_obstaculos_count;
    s_room_col.gatilhos         = collision_recepcao_gatilhos;
    s_room_col.gatilhos_count   = collision_recepcao_gatilhos_count;
    s_room_col.screen_w         = 480;
    s_room_col.screen_h         = 320;

    if (!load_all_assets()) {
        ESP_LOGE(TAG, "build abortado — assets do cartao SD indisponiveis "
                      "(cartao montou? arquivos /sd/assets/ copiados?)");
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

    /* L0 — piso: CHAO_01.png e 464x254, cobre o interior da sala apos
     * parede_esquerda (x=16) e parede_superior (y=65). */
    layer_full(s_game_layer, &s_assets[A_PISO].dsc, 16, 65);

    /* Instancia furniture + NPC + player a partir do JSON de layout no SD card.
     * asset_loader tem cache load-once: sprites que ja foram carregados
     * (NPC_IDLE, PLAYER) sao cache hit — sem double-load na PSRAM. */
    entity_t *player_entity = NULL;
    room_layout_spawn(s_game_layer, "recepcao", &player_entity);

    /* Obtem o lv_obj do player a partir da entidade spawnada. Converte pivot
     * bottom-center do JSON para top-left (lv_pos) usado pelo tick. */
    if (player_entity != NULL && player_entity->lv_obj != NULL) {
        s_player        = player_entity->lv_obj;
        s_player_entity = player_entity;
        s_px = player_entity->x - PLAYER_FRAME_W / 2;
        s_py = player_entity->y - PLAYER_FRAME_H;
    } else {
        /* Fallback manual (nao deve ocorrer se o JSON e o SD estiverem ok) */
        s_player        = lv_image_create(s_game_layer);
        s_player_entity = NULL;
        lv_image_set_src(s_player, &s_assets[A_PLAYER].dsc);
        lv_obj_set_size(s_player, PLAYER_FRAME_W, PLAYER_FRAME_H);
        lv_image_set_inner_align(s_player, LV_IMAGE_ALIGN_TOP_LEFT);
        no_scroll(s_player);
        s_px = 224; s_py = 184;   /* pivot(240,232) convertido */
    }

    /* Spawn do player ao voltar da Empresa — posicao exata de SPOWN_RETORNO_DO_ESCRITORIO
     * no INTERACOES.txt (460,168 = pivot bottom-center → lv_pos). */
    if (fsm_get_gameplay_sala_prev() == GAMEPLAY_SALA_EMPRESA) {
        s_px = 460 - PLAYER_FRAME_W / 2;
        s_py = 168 - PLAYER_FRAME_H;
    }

    {
        const collision_rect_t *g = room_gatilho_at(&s_room_col, &s_player_box, s_px, s_py);
        if (g && room_is_porta(g->kind)) {
            ESP_LOGW(TAG, "spawn (%d,%d) caiu em gatilho de porta — risco de loop", s_px, s_py);
        }
    }
    s_anim.dir = 1; s_anim.walk_idx = 1; s_anim.walk_ms = 0;
    s_a_cache = button_hal_peek(BTN_A);   /* evita edge fantasma no 1o tick */
    s_b_cache = button_hal_peek(BTN_B);
    lv_obj_set_pos(s_player, s_px, s_py);
    room_anim_step(&s_anim, s_player, 0, 0, 0, PLAYER_FRAME_W, PLAYER_FRAME_H);

    /* Encontra o NPC no pool e substitui o src pelo ponteiro estatico — seguro
     * para o tick (s_assets e estatico, persiste durante toda a tela). */
    s_npc = NULL;
    s_npc_facing = false;
    for (size_t i = 0; i < entity_pool_count(); i++) {
        entity_t *e = entity_pool_at(i);
        if (e->type == ENTITY_TYPE_NPC) {
            s_npc = e->lv_obj;
            if (s_npc) {
                lv_image_set_src(s_npc, &s_assets[A_NPC_IDLE].dsc);
            }
            break;
        }
    }

    /* Icone notif */
    s_icone = lv_image_create(s_ui_layer);
    lv_image_set_src(s_icone, &s_assets[A_ICONE].dsc);
    lv_obj_set_pos(s_icone, s_assets[A_ICONE].off_x, s_assets[A_ICONE].off_y);
    no_scroll(s_icone);
    s_icon_visible = true;
    s_icon_blink_ms = 0;

    /* Prompt "[A]" */
    s_prompt = lv_label_create(s_ui_layer);
    lv_label_set_text(s_prompt, "[A]");
    lv_obj_set_style_text_color(s_prompt, lv_color_hex(0xFFD000), LV_PART_MAIN);
    lv_obj_set_style_bg_color(s_prompt, lv_color_black(), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(s_prompt, LV_OPA_70, LV_PART_MAIN);
    lv_obj_set_style_pad_all(s_prompt, 2, LV_PART_MAIN);
    no_scroll(s_prompt);
    lv_obj_add_flag(s_prompt, LV_OBJ_FLAG_HIDDEN);

    /* === Dialogo do recepcionista (overlay, oculto por default) === */
    s_dlg_box = lv_image_create(s_ui_layer);
    lv_image_set_src(s_dlg_box, &s_assets[A_CAIXA_DIALOGO].dsc);
    lv_obj_set_pos(s_dlg_box, s_assets[A_CAIXA_DIALOGO].off_x,
                              s_assets[A_CAIXA_DIALOGO].off_y);
    no_scroll(s_dlg_box);
    lv_obj_add_flag(s_dlg_box, LV_OBJ_FLAG_HIDDEN);

    s_dlg_text = lv_label_create(s_ui_layer);
    lv_label_set_text(s_dlg_text, "");
    lv_label_set_long_mode(s_dlg_text, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(s_dlg_text, s_assets[A_CAIXA_TEXTO].dsc.header.w - 8);
    lv_obj_set_pos(s_dlg_text, s_assets[A_CAIXA_TEXTO].off_x + 4,
                               s_assets[A_CAIXA_TEXTO].off_y + 4);
    lv_obj_set_style_text_color(s_dlg_text, lv_color_white(), LV_PART_MAIN);
    no_scroll(s_dlg_text);
    lv_obj_add_flag(s_dlg_text, LV_OBJ_FLAG_HIDDEN);

    s_dlg_hint = lv_label_create(s_ui_layer);
    lv_label_set_text(s_dlg_hint, "[A] >>  [B] Pular");
    lv_obj_set_style_text_color(s_dlg_hint, lv_color_hex(0xFFA500), LV_PART_MAIN);
    lv_obj_set_pos(s_dlg_hint,
                   s_assets[A_CAIXA_DIALOGO].off_x + s_assets[A_CAIXA_DIALOGO].dsc.header.w - 130,
                   s_assets[A_CAIXA_DIALOGO].off_y + s_assets[A_CAIXA_DIALOGO].dsc.header.h - 18);
    no_scroll(s_dlg_hint);
    lv_obj_add_flag(s_dlg_hint, LV_OBJ_FLAG_HIDDEN);

    s_dlg_state = DLG_INACTIVE;
    s_dlg_played = false;
    s_dlg_line = 0;
    s_dlg_char = 0;

    screen_hud_build(s_ui_layer);

    s_timer = lv_timer_create(recepcao_tick, UI_TICK_MS, NULL);
    ESP_LOGI(TAG, "recepcao built (player @ %d,%d)", s_px, s_py);
}

void screen_recepcao_destroy(void)
{
    if (s_timer) { lv_timer_delete(s_timer); s_timer = NULL; }
    screen_hud_destroy();
    /* Limpa entidades antes do lv_obj_delete(s_root) para evitar double-delete
     * pelo cascade do parent nos lv_objs das entidades. */
    entity_pool_clear();
    s_player        = NULL;
    s_player_entity = NULL;
    s_npc           = NULL;
    if (s_root) {
        lv_obj_delete(s_root);
        s_root = NULL;
        s_game_layer = s_ui_layer = NULL;
        s_icone = s_prompt = NULL;
        s_dlg_box = s_dlg_text = s_dlg_hint = NULL;
    }
    free_all_assets();
    dialog_loader_free(&s_dialogo);
}
