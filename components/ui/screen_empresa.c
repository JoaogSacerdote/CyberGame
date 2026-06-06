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

static const char *TAG = "UI_EMPRESA";

/* Spawn fallback — perto da porta esquerda (entrada vinda da Recepcao).
 * Usado se a tabela AREA_SPAWN nao existir OU se o spawn da tabela cair
 * em cima de um obstaculo. */
#define SPAWN_FALLBACK_X 40
#define SPAWN_FALLBACK_Y 160

static int16_t s_px = SPAWN_FALLBACK_X;
static int16_t s_py = SPAWN_FALLBACK_Y;
/* Entra olhando pra RIGHT (vindo da Recepcao). */
static room_player_anim_t s_anim = { .dir = 2, .walk_idx = 1, .walk_ms = 0 };

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

static button_state_t s_a_cache  = BTN_RELEASED;
static button_state_t s_b_cache  = BTN_RELEASED;

/* === Assets da tela, carregados do SD card no build e liberados no destroy === */
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

/* Player bbox para colisao: pes (16x12) com offset (8, 36) no frame. */
static const room_player_box_t s_player_box = {
    .off_x = 8, .off_y = 36, .w = 16, .h = 12,
};

/* Dados de colisao da sala — preenchidos no build. */
static room_collision_t s_room_col;

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

    /* PAUSE como overlay: a tela continua viva por baixo, mas o tick
     * congela. Nem movimento nem animacao nem leitura de entrada — assim
     * o estado da tela e identico antes/depois do pause. */
    if (fsm_get_state() == GAME_STATE_PAUSE) return;

    /* Atualiza HUD (clock). Diff-gated internamente — barato. */
    screen_hud_tick();

    /* HAL devolve jx+ = direita, jy+ = baixo (casa com coords LVGL). */
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

    /* Gatilho sob o player */
    const collision_rect_t *g = room_gatilho_at(&s_room_col, &s_player_box, s_px, s_py);

    /* Sincroniza com a FSM se o player esta em um gatilho de equipamento.
     * Sem isso, a sub-FSM dispararia TERMINAL_ABERTO em qualquer Y. */
    fsm_set_player_at_equipment(g && g->kind == AREA_TAREFA_VERDE);

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

    /* Porta: troca de sala por CONTATO. Spawn ja nasce afastado dos gatilhos
     * de porta (ver SPAWN_DOOR_MARGIN_PX), entao nao precisa de flag de armado. */
    if (g && g->kind == AREA_PORTA_RECEPCAO) {
        ESP_LOGI(TAG, "porta recepcao (contato) -> trocando sala");
        fsm_set_gameplay_sala(GAMEPLAY_SALA_RECEPCAO);
        return;
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
            if (!room_collides_at(&s_room_col, &s_player_box, r->x, r->y)) {
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

/* Carrega do SD card todos os assets da tela. Em falha, desfaz os que ja
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
    /* Aponta os helpers de colisao pras tabelas da Empresa. */
    s_room_col.obstaculos       = collision_empresa_obstaculos;
    s_room_col.obstaculos_count = collision_empresa_obstaculos_count;
    s_room_col.gatilhos         = collision_empresa_gatilhos;
    s_room_col.gatilhos_count   = collision_empresa_gatilhos_count;
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

    layer_full(s_root, &s_assets[A_PISO].dsc, 0, 0);
    layer_full(s_root, &s_assets[A_PAREDES].dsc, 0, 0);

    s_player = lv_image_create(s_root);
    lv_image_set_src(s_player, &s_assets[A_PLAYER].dsc);
    lv_obj_set_size(s_player, PLAYER_FRAME_W, PLAYER_FRAME_H);
    lv_image_set_inner_align(s_player, LV_IMAGE_ALIGN_TOP_LEFT);
    no_scroll(s_player);
    apply_spawn_from_table();
    /* Sanity: se o spawn cair em gatilho de porta (mapa mal configurado),
     * vamos logar — sem isso, vira loop silencioso de troca de sala. */
    {
        const collision_rect_t *g = room_gatilho_at(&s_room_col, &s_player_box, s_px, s_py);
        if (g && g->kind == AREA_PORTA_RECEPCAO) {
            ESP_LOGW(TAG, "spawn (%d,%d) caiu em gatilho de porta — risco de loop", s_px, s_py);
        }
    }
    s_anim.dir = 2; s_anim.walk_idx = 1; s_anim.walk_ms = 0;   /* RIGHT */
    s_tarefa_open = false;
    s_a_cache = button_hal_peek(BTN_A);
    s_b_cache = button_hal_peek(BTN_B);
    lv_obj_set_pos(s_player, s_px, s_py);
    room_anim_step(&s_anim, s_player, 0, 0, 0, PLAYER_FRAME_W, PLAYER_FRAME_H);

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

    /* HUD persistente no topo. Eh filho de s_root pra cair junto no destroy. */
    screen_hud_build(s_root);

    s_timer = lv_timer_create(empresa_tick, UI_TICK_MS, NULL);
    ESP_LOGI(TAG, "empresa built (player @ %d,%d)", s_px, s_py);
}

void screen_empresa_destroy(void)
{
    if (s_timer) { lv_timer_delete(s_timer); s_timer = NULL; }
    /* HUD primeiro (filho de s_root) — nullifica os ponteiros internos
     * antes que o delete cascateado do parent invalide os handles. */
    screen_hud_destroy();
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
