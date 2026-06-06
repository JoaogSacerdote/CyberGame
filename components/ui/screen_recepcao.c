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

static const char *TAG = "UI_RECEPCAO";

/* Spawn — meio da sala, em cima do tapete, LONGE das areas de gatilho
 * (PORTA_EMPRESA em 135,254 e INTERACAO_NPC em 137,261). Anteriormente
 * estava em (155, 215) e caia DENTRO de ambas as areas — player
 * teletransportava pra Empresa no primeiro tick. */
#define SPAWN_X 240
#define SPAWN_Y 200

/* === Dialogo do recepcionista ===
 * Carregado do SD card no build (assets/dialogos/recepcionista.txt -> blob).
 * Edicao do texto vive em arquivo .txt + regravar o SD, sem rebuild do firmware. */
static dialog_t s_dialogo;

typedef enum {
    DLG_INACTIVE = 0,
    DLG_TYPING,
    DLG_WAITING,    /* texto completo, esperando A pra avancar */
} dlg_state_t;

/* Posicao do player persistente entre destroys (pra preservar quando troca
 * de sala e volta) — opcional. Por enquanto reset no build. */
static int16_t s_px = SPAWN_X;
static int16_t s_py = SPAWN_Y;
static room_player_anim_t s_anim = { .dir = 0, .walk_idx = 1, .walk_ms = 0 };
static bool    s_npc_facing = false;
static bool    s_icon_visible = true;
static uint32_t s_icon_blink_ms = 0;

static lv_obj_t   *s_root        = NULL;
static lv_obj_t   *s_player      = NULL;
static lv_obj_t   *s_npc         = NULL;
static lv_obj_t   *s_icone       = NULL;
static lv_obj_t   *s_prompt      = NULL;   /* "[A]" — aparece sobre o player perto de gatilho */
static lv_obj_t   *s_dlg_box     = NULL;   /* PNG caixa de dialogo (oculto por default) */
static lv_obj_t   *s_dlg_text    = NULL;   /* texto typewriter */
static lv_obj_t   *s_dlg_hint    = NULL;   /* "[A] >>  [B] Pular" */
static lv_timer_t *s_timer       = NULL;

/* === Assets da tela, carregados do SD card no build e liberados no destroy === */
typedef enum {
    A_PISO = 0, A_PAREDES, A_PLAYER, A_COMPLEMENTO,
    A_RECEP_IDLE, A_RECEP_DIALOG, A_ICONE, A_CAIXA_DIALOGO, A_CAIXA_TEXTO,
    A_COUNT
} rec_slot_t;

static const uint16_t REC_ASSET_ID[A_COUNT] = {
    [A_PISO]          = ASSET_REC_PISO,
    [A_PAREDES]       = ASSET_REC_PAREDES,
    [A_PLAYER]        = ASSET_PLAYER,
    [A_COMPLEMENTO]   = ASSET_REC_COMPLEMENTO,
    [A_RECEP_IDLE]    = ASSET_REC_RECEP_IDLE,
    [A_RECEP_DIALOG]  = ASSET_REC_RECEP_DIALOG,
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
        /* Atualiza label com substring [0..s_dlg_char]. lv_label_set_text_fmt
         * aloca interno conforme o tamanho — elimina o limite arbitrario de
         * buffer estatico (truncamento silencioso anterior em falas >255). */
        lv_label_set_text_fmt(s_dlg_text, "%.*s", (int)s_dlg_char, full);
    }
}

static void recepcao_tick(lv_timer_t *t)
{
    (void)t;

    /* Guard defensivo: se a tela foi destruida mas o timer ainda
     * disparou, aborta. */
    if (!s_root || !s_player) return;

    /* PAUSE como overlay: a tela continua viva por baixo, mas o tick
     * congela. Nem movimento nem animacao nem leitura de entrada — assim
     * o estado da tela e identico antes/depois do pause. */
    if (fsm_get_state() == GAME_STATE_PAUSE) return;

    /* Atualiza HUD (clock). Diff-gated internamente — barato. */
    screen_hud_tick();

    /* Se dialogo ativo, processa input do dialogo e BLOQUEIA movimento. */
    if (s_dlg_state != DLG_INACTIVE) {
        dlg_tick(UI_TICK_MS);
        return;
    }

    /* HAL devolve jx+ = direita, jy+ = baixo (casa com coords LVGL). */
    const joystick_data_t j = joystick_hal_get_state();
    const int jx = j.x;
    const int jy = j.y;
    const int sx_mag = room_speed_from_mag(jx < 0 ? -jx : jx);
    const int sy_mag = room_speed_from_mag(jy < 0 ? -jy : jy);
    const int dx = (sx_mag == 0) ? 0 : (jx > 0 ? +1 : -1);
    const int dy = (sy_mag == 0) ? 0 : (jy > 0 ? +1 : -1);

    room_anim_update_dir(&s_anim, jx, jy);

    /* Move com colisao por eixo separado (raspar parede) */
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

    /* NPC muda de pose por proximidade (feedback visual — automatico). */
    const bool near_npc = (g && g->kind == AREA_INTERACAO_NPC);
    if (near_npc != s_npc_facing) {
        s_npc_facing = near_npc;
        if (near_npc) {
            lv_image_set_src(s_npc, &s_assets[A_RECEP_DIALOG].dsc);
            lv_obj_set_pos(s_npc, s_assets[A_RECEP_DIALOG].off_x,
                                  s_assets[A_RECEP_DIALOG].off_y);
        } else {
            lv_image_set_src(s_npc, &s_assets[A_RECEP_IDLE].dsc);
            lv_obj_set_pos(s_npc, s_assets[A_RECEP_IDLE].off_x,
                                  s_assets[A_RECEP_IDLE].off_y);
        }
    }

    /* Porta: troca de sala por CONTATO (sem precisar apertar A). Spawn ja
     * nasce afastado dos gatilhos de porta (ver SPAWN_DOOR_MARGIN_PX), entao
     * nao precisa de flag de armado. */
    if (g && g->kind == AREA_PORTA_EMPRESA) {
        ESP_LOGI(TAG, "porta empresa (contato) -> trocando sala");
        fsm_set_gameplay_sala(GAMEPLAY_SALA_EMPRESA);
        return;
    }

    /* Prompt "[A]": aparece sobre o player so para gatilhos INTERATIVOS
     * (NPC, tarefas) — portas trocam por contato, sem prompt. */
    if (g && !room_is_porta(g->kind)) {
        lv_obj_set_pos(s_prompt, s_px + 8, s_py - 17);
        lv_obj_remove_flag(s_prompt, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(s_prompt, LV_OBJ_FLAG_HIDDEN);
    }

    /* Icone notif: pisca 500ms on/off ate o jogador interagir com o NPC. */
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

    /* Botao A: interage com gatilhos INTERATIVOS (NPC). */
    if (ui_btn_edge(BTN_A, &s_a_cache) && g && g->kind == AREA_INTERACAO_NPC) {
        if (!s_dlg_played) {
            s_icon_visible = false;
            lv_obj_add_flag(s_icone, LV_OBJ_FLAG_HIDDEN);
            s_b_cache = button_hal_peek(BTN_B);
            dlg_start();
            return;   /* dialogo assume o controle no proximo tick */
        }
    }
}

/* Remove scrollabilidade — defensivo contra crash em lv_obj_readjust_scroll
 * durante o layout update do refresh. */
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

/* Carrega do SD card todos os assets da tela + dialogo. Em falha, desfaz os
 * que ja subiram e retorna false. */
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
    const esp_err_t derr = dialog_loader_load(ASSET_DIALOG_RECEP, &s_dialogo);
    if (derr != ESP_OK) {
        ESP_LOGE(TAG, "dialog_loader_load falhou: %s", esp_err_to_name(derr));
        free_all_assets();
        return false;
    }
    return true;
}

void screen_recepcao_build(void)
{
    /* Aponta os helpers de colisao pras tabelas da Recepcao. */
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

    /* L0 — piso (cobre 480x320) */
    layer_full(s_root, &s_assets[A_PISO].dsc, 0, 0);

    /* L2 — paredes/objetos (cobre 480x320) */
    layer_full(s_root, &s_assets[A_PAREDES].dsc, 0, 0);

    /* PLAYER — entre paredes e complemento. Janela 32x48 mostra so 1 frame
     * do sheet 96x192 via offset_x/y. */
    s_player = lv_image_create(s_root);
    lv_image_set_src(s_player, &s_assets[A_PLAYER].dsc);
    lv_obj_set_size(s_player, PLAYER_FRAME_W, PLAYER_FRAME_H);
    lv_image_set_inner_align(s_player, LV_IMAGE_ALIGN_TOP_LEFT);
    no_scroll(s_player);

    /* Spawn: se voltou da Empresa, nasce A ESQUERDA da porta com margem
     * suficiente pra hitbox do player NAO tocar o gatilho da porta nem
     * mesmo com o passo maximo do joystick no primeiro tick. */
    if (fsm_get_gameplay_sala_prev() == GAMEPLAY_SALA_EMPRESA) {
        const collision_rect_t *porta = room_find_gatilho(&s_room_col, AREA_PORTA_EMPRESA);
        if (porta) {
            s_px = porta->x - PLAYER_FRAME_W - SPAWN_DOOR_MARGIN_PX;
            s_py = porta->y + porta->h / 2 - PLAYER_FRAME_H / 2;
            if (room_collides_at(&s_room_col, &s_player_box, s_px, s_py)) {
                s_px -= 12;
            }
        } else {
            s_px = SPAWN_X; s_py = SPAWN_Y;
        }
    } else {
        s_px = SPAWN_X; s_py = SPAWN_Y;
    }
    /* Sanity: se o spawn cair em gatilho de porta (mapa mal configurado),
     * vamos logar — sem isso, vira loop silencioso. */
    {
        const collision_rect_t *g = room_gatilho_at(&s_room_col, &s_player_box, s_px, s_py);
        if (g && room_is_porta(g->kind)) {
            ESP_LOGW(TAG, "spawn (%d,%d) caiu em gatilho de porta — risco de loop", s_px, s_py);
        }
    }
    s_anim.dir = 1; s_anim.walk_idx = 1; s_anim.walk_ms = 0;   /* LEFT (saiu da porta) */
    lv_obj_set_pos(s_player, s_px, s_py);
    /* Aplica frame inicial parado — passa dx=dy=0 pra forcar idle. */
    room_anim_step(&s_anim, s_player, 0, 0, 0, PLAYER_FRAME_W, PLAYER_FRAME_H);

    /* L3 — complemento (em cima do player; cropado, posicao via meta) */
    layer_full(s_root, &s_assets[A_COMPLEMENTO].dsc,
               s_assets[A_COMPLEMENTO].off_x, s_assets[A_COMPLEMENTO].off_y);

    /* L5 — recepcionista (idle por padrao) */
    s_npc = lv_image_create(s_root);
    lv_image_set_src(s_npc, &s_assets[A_RECEP_IDLE].dsc);
    lv_obj_set_pos(s_npc, s_assets[A_RECEP_IDLE].off_x,
                          s_assets[A_RECEP_IDLE].off_y);
    no_scroll(s_npc);
    s_npc_facing = false;

    /* L4 — icone notif (em cima da recepcionista, fica acima do NPC pelo
     * z-order de criacao) */
    s_icone = lv_image_create(s_root);
    lv_image_set_src(s_icone, &s_assets[A_ICONE].dsc);
    lv_obj_set_pos(s_icone, s_assets[A_ICONE].off_x,
                            s_assets[A_ICONE].off_y);
    no_scroll(s_icone);
    s_icon_visible = true;
    s_icon_blink_ms = 0;

    /* Prompt "[A]" — segue o player quando ha gatilho. Oculto por padrao. */
    s_prompt = lv_label_create(s_root);
    lv_label_set_text(s_prompt, "[A]");
    lv_obj_set_style_text_color(s_prompt, lv_color_hex(0xFFD000), LV_PART_MAIN);
    lv_obj_set_style_bg_color(s_prompt, lv_color_black(), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(s_prompt, LV_OPA_70, LV_PART_MAIN);
    lv_obj_set_style_pad_all(s_prompt, 2, LV_PART_MAIN);
    no_scroll(s_prompt);
    lv_obj_add_flag(s_prompt, LV_OBJ_FLAG_HIDDEN);

    /* === Dialogo do recepcionista (overlay, oculto por default) === */
    /* Camada visivel: a PNG da caixa em (84, 24) tamanho 323x107 */
    s_dlg_box = lv_image_create(s_root);
    lv_image_set_src(s_dlg_box, &s_assets[A_CAIXA_DIALOGO].dsc);
    lv_obj_set_pos(s_dlg_box, s_assets[A_CAIXA_DIALOGO].off_x,
                              s_assets[A_CAIXA_DIALOGO].off_y);
    no_scroll(s_dlg_box);
    lv_obj_add_flag(s_dlg_box, LV_OBJ_FLAG_HIDDEN);

    /* Label do texto: dentro da regiao da CAIXA_DELIMITADORA_TEXTO_08
     * (156, 32, 231, 74). Margem interna pequena. */
    s_dlg_text = lv_label_create(s_root);
    lv_label_set_text(s_dlg_text, "");
    lv_label_set_long_mode(s_dlg_text, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(s_dlg_text, s_assets[A_CAIXA_TEXTO].dsc.header.w - 8);
    lv_obj_set_pos(s_dlg_text, s_assets[A_CAIXA_TEXTO].off_x + 4,
                               s_assets[A_CAIXA_TEXTO].off_y + 4);
    lv_obj_set_style_text_color(s_dlg_text, lv_color_white(), LV_PART_MAIN);
    no_scroll(s_dlg_text);
    lv_obj_add_flag(s_dlg_text, LV_OBJ_FLAG_HIDDEN);

    /* Hint "[A] >>  [B] Pular" no canto inf-direito da caixa */
    s_dlg_hint = lv_label_create(s_root);
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

    /* HUD persistente no topo. Eh filho de s_root pra cair junto no destroy. */
    screen_hud_build(s_root);

    s_timer = lv_timer_create(recepcao_tick, UI_TICK_MS, NULL);
    ESP_LOGI(TAG, "recepcao built (player @ %d,%d)", s_px, s_py);
}

void screen_recepcao_destroy(void)
{
    if (s_timer) { lv_timer_delete(s_timer); s_timer = NULL; }
    /* HUD primeiro (filho de s_root) — nullifica os ponteiros internos
     * antes que o delete cascateado do parent invalide os handles. */
    screen_hud_destroy();
    if (s_root)  {
        lv_obj_delete(s_root);
        s_root = NULL;
        s_player = s_npc = s_icone = s_prompt = NULL;
        s_dlg_box = s_dlg_text = s_dlg_hint = NULL;
    }
    /* Libera os pixels da PSRAM DEPOIS de deletar os objetos LVGL que
     * apontavam para eles. */
    free_all_assets();
    dialog_loader_free(&s_dialogo);
}
