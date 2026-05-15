#include "ui.h"
#include "ui_internal.h"

#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include "esp_log.h"
#include "lvgl.h"
#include "assets.h"
#include "collision_data.h"
#include "joystick_hal.h"
#include "button_hal.h"
#include "fsm_gameplay.h"
#include "fsm.h"

static const char *TAG = "UI_RECEPCAO";

/* Cada frame do sprite-sheet */
#define PFRAME_W   32
#define PFRAME_H   48
#define J_DEADZONE 30               /* fora do +-30 considera deflexao */
#define PSTEP_MIN  2                /* px/tick com deflexao minima (-20% do valor anterior) */
#define PSTEP_MAX  6                /* px/tick com deflexao maxima (-20% do valor anterior) */

/* Velocidade proporcional a deflexao do joystick (RESPOSTAS.txt 9):
 * mag = |eixo| em 0..100 -> step em PSTEP_MIN..PSTEP_MAX px/tick. */
static int speed_from_mag(int mag)
{
    if (mag <= J_DEADZONE) return 0;
    int s = PSTEP_MIN + (mag - J_DEADZONE) * (PSTEP_MAX - PSTEP_MIN) / (100 - J_DEADZONE);
    if (s < PSTEP_MIN) s = PSTEP_MIN;
    if (s > PSTEP_MAX) s = PSTEP_MAX;
    return s;
}

/* Anim walk: ordem das colunas (3 valida frames + idle de volta) */
static const int8_t WALK_SEQ[] = { 0, 1, 2, 1 };
#define WALK_PERIOD_MS 125          /* ~8 fps */

/* Spawn — meio da sala, em cima do tapete, LONGE das areas de gatilho
 * (PORTA_EMPRESA em 135,254 e INTERACAO_NPC em 137,261). Anteriormente
 * estava em (155, 215) e caia DENTRO de ambas as areas — player
 * teletransportava pra Empresa no primeiro tick. */
#define SPAWN_X 240
#define SPAWN_Y 200

/* === Dialogo do recepcionista ===
 * Textos em PT-BR (sem acentos enquanto a fonte padrao do LVGL nao
 * suporta — quando trocar pra pixel art PT-BR, posso reverter aos
 * acentos do DIALOGO.txt original). */
static const char *DIALOGO[] = {
    "Bom dia! Voce deve ser o novo analista de ciberseguranca contratado, correto? Bem-vindo a empresa.",
    "Seu turno e das 10h as 18h. Durante esse periodo voce vai encontrar 3 tipos de tarefas.",
    "As tarefas VERDES sao de prevencao. Sao opcionais e tranquilas.",
    "Eu recomendo sempre fazer-las, isso evita problemas maiores no futuro.",
    "As tarefas AMARELAS sao anomalias. Elas podem evoluir para algo pior, se ignoradas.",
    "As tarefas VERMELHAS sao ataques ativos. Quando aparecerem, aja imediatamente.",
    "Isso e tudo, verifique se ha mais ocorrencias pelo andar, a sala 2 fica logo ao seu lado esquerdo.",
};
#define DIALOGO_N (sizeof(DIALOGO) / sizeof(DIALOGO[0]))
#define TYPE_PERIOD_MS 30   /* 1 caractere a cada 30ms = ~33 cps */

typedef enum {
    DLG_INACTIVE = 0,
    DLG_TYPING,
    DLG_WAITING,    /* texto completo, esperando A pra avancar */
} dlg_state_t;

/* Posicao do player persistente entre destroys (pra preservar quando troca
 * de sala e volta) — opcional. Por enquanto reset no build. */
static int16_t s_px = SPAWN_X;
static int16_t s_py = SPAWN_Y;
static int8_t  s_dir = 0;           /* linha do sheet: 0=DOWN, 1=LEFT, 2=RIGHT, 3=UP */
static uint8_t s_walk_idx = 1;      /* indice em WALK_SEQ */
static uint32_t s_walk_ms = 0;
static bool    s_npc_facing = false;
static bool    s_icon_visible = true;
static uint32_t s_icon_blink_ms = 0;
/* Anti-loop: ao spawnar em cima de uma porta (retorno da Empresa), a troca
 * de sala fica "desarmada" ate o player SAIR da area da porta. */
static bool    s_porta_armed = false;

static lv_obj_t   *s_root        = NULL;
static lv_obj_t   *s_player      = NULL;
static lv_obj_t   *s_npc         = NULL;
static lv_obj_t   *s_icone       = NULL;
static lv_obj_t   *s_prompt      = NULL;   /* "[A]" — aparece sobre o player perto de gatilho */
static lv_obj_t   *s_dlg_box     = NULL;   /* PNG caixa de dialogo (oculto por default) */
static lv_obj_t   *s_dlg_text    = NULL;   /* texto typewriter */
static lv_obj_t   *s_dlg_hint    = NULL;   /* "[A] >>  [B] Pular" */
static lv_timer_t *s_timer       = NULL;

/* Estado do dialogo */
static dlg_state_t s_dlg_state = DLG_INACTIVE;
static bool        s_dlg_played = false;   /* nao re-toca dialogo apos jogador ja ter visto */
static uint8_t     s_dlg_line   = 0;
static uint16_t    s_dlg_char   = 0;
static uint32_t    s_dlg_typewriter_ms = 0;
static button_state_t s_a_cache = BTN_RELEASED;
static button_state_t s_b_cache = BTN_RELEASED;

/* Player bbox para colisao: pes (16x12 na base) — o sprite e 32x48 mas o "pe"
 * que toca o chao e bem menor. Usamos 16x12 centralizado em (off_x+16, off_y+36). */
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
    for (size_t i = 0; i < collision_recepcao_obstaculos_count; ++i) {
        const collision_rect_t *r = &collision_recepcao_obstaculos[i];
        if (rects_overlap(cx, cy, PCOL_W, PCOL_H, r->x, r->y, r->w, r->h)) {
            return true;
        }
    }
    /* Mantem player dentro da tela */
    if (cx < 0 || cy < 0 || cx + PCOL_W > 480 || cy + PCOL_H > 320) return true;
    return false;
}

static const collision_rect_t *gatilho_at(int px, int py)
{
    const int cx = px + PCOL_OFF_X;
    const int cy = py + PCOL_OFF_Y;
    for (size_t i = 0; i < collision_recepcao_gatilhos_count; ++i) {
        const collision_rect_t *r = &collision_recepcao_gatilhos[i];
        if (rects_overlap(cx, cy, PCOL_W, PCOL_H, r->x, r->y, r->w, r->h)) {
            return r;
        }
    }
    return NULL;
}

static const collision_rect_t *find_gatilho(collision_kind_t kind)
{
    for (size_t i = 0; i < collision_recepcao_gatilhos_count; ++i) {
        if (collision_recepcao_gatilhos[i].kind == kind) {
            return &collision_recepcao_gatilhos[i];
        }
    }
    return NULL;
}

static bool is_porta(collision_kind_t k)
{
    return k == AREA_PORTA_EMPRESA || k == AREA_PORTA_RECEPCAO;
}

static void apply_player_frame(void)
{
    const int8_t col = WALK_SEQ[s_walk_idx];
    lv_image_set_offset_x(s_player, -col * PFRAME_W);
    lv_image_set_offset_y(s_player, -s_dir * PFRAME_H);
}

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
    lv_label_set_text(s_dlg_text, DIALOGO[s_dlg_line]);
    s_dlg_char = strlen(DIALOGO[s_dlg_line]);
    s_dlg_state = DLG_WAITING;
    lv_obj_remove_flag(s_dlg_hint, LV_OBJ_FLAG_HIDDEN);
}

static void dlg_next_line(void)
{
    s_dlg_line++;
    if (s_dlg_line >= DIALOGO_N) {
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
    while (s_dlg_typewriter_ms >= TYPE_PERIOD_MS && s_dlg_state == DLG_TYPING) {
        s_dlg_typewriter_ms -= TYPE_PERIOD_MS;
        const char *full = DIALOGO[s_dlg_line];
        const size_t total = strlen(full);
        if (s_dlg_char >= total) {
            s_dlg_state = DLG_WAITING;
            lv_obj_remove_flag(s_dlg_hint, LV_OBJ_FLAG_HIDDEN);
            break;
        }
        s_dlg_char++;
        /* Atualiza label com substring [0..s_dlg_char] */
        char buf[256];
        size_t n = (s_dlg_char < sizeof(buf) - 1) ? s_dlg_char : sizeof(buf) - 1;
        memcpy(buf, full, n);
        buf[n] = '\0';
        lv_label_set_text(s_dlg_text, buf);
    }
}

static void recepcao_tick(lv_timer_t *t)
{
    (void)t;

    /* Guard defensivo: se a tela foi destruida mas o timer ainda
     * disparou, aborta. */
    if (!s_root || !s_player) return;

    /* Se dialogo ativo, processa input do dialogo e BLOQUEIA movimento. */
    if (s_dlg_state != DLG_INACTIVE) {
        dlg_tick(UI_TICK_MS);
        return;
    }

    /* Leitura do joystick — o eixo X chega do joystick_hal com sinal trocado
     * em relacao a tela; invertemos so o X. jx>0=direita, jy>0=baixo.
     * Velocidade proporcional a deflexao. */
    const joystick_data_t j = joystick_hal_get_state();
    const int jx = -j.x;
    const int jy = j.y;
    int dx = 0, dy = 0, sx = 0, sy = 0;
    if (jx >  J_DEADZONE) { dx = +1; sx = speed_from_mag(jx); }
    else if (jx < -J_DEADZONE) { dx = -1; sx = speed_from_mag(-jx); }
    if (jy >  J_DEADZONE) { dy = +1; sy = speed_from_mag(jy); }
    else if (jy < -J_DEADZONE) { dy = -1; sy = speed_from_mag(-jy); }

    /* Atualiza direcao do sprite — eixo dominante.
     * Linhas do sheet: 0=DOWN, 1=LEFT, 2=RIGHT, 3=UP. */
    if (abs(jx) > abs(jy)) {
        if (dx != 0) s_dir = (dx > 0) ? 2 /*RIGHT*/ : 1 /*LEFT*/;
    } else if (dy != 0) {
        s_dir = (dy > 0) ? 0 /*DOWN*/ : 3 /*UP*/;
    }

    /* Move com colisao por eixo separado (raspar parede) */
    if (dx != 0) {
        const int nx = s_px + dx * sx;
        if (!collides_at(nx, s_py)) s_px = nx;
    }
    if (dy != 0) {
        const int ny = s_py + dy * sy;
        if (!collides_at(s_px, ny)) s_py = ny;
    }

    /* Anim walk */
    if (dx != 0 || dy != 0) {
        s_walk_ms += UI_TICK_MS;
        if (s_walk_ms >= WALK_PERIOD_MS) {
            s_walk_ms = 0;
            s_walk_idx = (s_walk_idx + 1) % (sizeof(WALK_SEQ) / sizeof(WALK_SEQ[0]));
        }
    } else {
        s_walk_idx = 1;  /* idle */
        s_walk_ms = 0;
    }

    lv_obj_set_pos(s_player, s_px, s_py);
    apply_player_frame();

    /* Gatilho sob o player */
    const collision_rect_t *g = gatilho_at(s_px, s_py);

    /* NPC muda de pose por proximidade (feedback visual — automatico). */
    const bool near_npc = (g && g->kind == AREA_INTERACAO_NPC);
    if (near_npc != s_npc_facing) {
        s_npc_facing = near_npc;
        if (near_npc) {
            lv_image_set_src(s_npc, &img_rec_recep_dialog);
            lv_obj_set_pos(s_npc, IMG_REC_RECEP_DIALOG_META.off_x,
                                  IMG_REC_RECEP_DIALOG_META.off_y);
        } else {
            lv_image_set_src(s_npc, &img_rec_recep_idle);
            lv_obj_set_pos(s_npc, IMG_REC_RECEP_IDLE_META.off_x,
                                  IMG_REC_RECEP_IDLE_META.off_y);
        }
    }

    /* Porta: troca de sala por CONTATO (sem precisar apertar A). O
     * s_porta_armed evita loop quando o player spawna em cima da porta. */
    if (g && is_porta(g->kind)) {
        if (s_porta_armed && g->kind == AREA_PORTA_EMPRESA) {
            ESP_LOGI(TAG, "porta empresa (contato) -> trocando sala");
            fsm_set_gameplay_sala(GAMEPLAY_SALA_EMPRESA);
            return;
        }
    } else {
        s_porta_armed = true;  /* saiu de qualquer porta -> rearma */
    }

    /* Prompt "[A]": aparece sobre o player so para gatilhos INTERATIVOS
     * (NPC, tarefas) — portas trocam por contato, sem prompt. */
    if (g && !is_porta(g->kind)) {
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

void screen_recepcao_build(void)
{
    s_root = lv_obj_create(lv_screen_active());
    lv_obj_set_size(s_root, 480, 320);
    lv_obj_set_pos(s_root, 0, 0);
    lv_obj_set_style_bg_color(s_root, lv_color_black(), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(s_root, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(s_root, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(s_root, 0, LV_PART_MAIN);
    no_scroll(s_root);

    /* L0 — piso (cobre 480x320) */
    layer_full(s_root, &img_rec_piso, 0, 0);

    /* L2 — paredes/objetos (cobre 480x320) */
    layer_full(s_root, &img_rec_paredes, 0, 0);

    /* PLAYER — entre paredes e complemento. Janela 32x48 mostra so 1 frame
     * do sheet 96x192 via offset_x/y. */
    s_player = lv_image_create(s_root);
    lv_image_set_src(s_player, &img_player);
    lv_obj_set_size(s_player, PFRAME_W, PFRAME_H);
    lv_image_set_inner_align(s_player, LV_IMAGE_ALIGN_TOP_LEFT);
    no_scroll(s_player);

    /* Spawn: se voltou da Empresa, nasce A ESQUERDA da porta (fora da area
     * de gatilho, pra nao re-disparar a troca de sala); caso contrario
     * (entrada inicial), usa o ponto fixo. */
    if (fsm_get_gameplay_sala_prev() == GAMEPLAY_SALA_EMPRESA) {
        const collision_rect_t *porta = find_gatilho(AREA_PORTA_EMPRESA);
        if (porta) {
            s_px = porta->x - PFRAME_W;
            s_py = porta->y + porta->h / 2 - PFRAME_H / 2;
            if (collides_at(s_px, s_py)) {
                s_px = porta->x - PFRAME_W - 12;
            }
        } else {
            s_px = SPAWN_X; s_py = SPAWN_Y;
        }
    } else {
        s_px = SPAWN_X; s_py = SPAWN_Y;
    }
    /* Nasce fora de qualquer porta — rearma no 1o tick que estiver livre. */
    s_porta_armed = false;
    s_dir = 1; s_walk_idx = 1; s_walk_ms = 0;   /* olhando pra LEFT (saiu da porta) */
    lv_obj_set_pos(s_player, s_px, s_py);
    apply_player_frame();

    /* L3 — complemento (em cima do player; cropado, posicao via meta) */
    layer_full(s_root, &img_rec_complemento,
               IMG_REC_COMPLEMENTO_META.off_x, IMG_REC_COMPLEMENTO_META.off_y);

    /* L5 — recepcionista (idle por padrao) */
    s_npc = lv_image_create(s_root);
    lv_image_set_src(s_npc, &img_rec_recep_idle);
    lv_obj_set_pos(s_npc, IMG_REC_RECEP_IDLE_META.off_x,
                          IMG_REC_RECEP_IDLE_META.off_y);
    no_scroll(s_npc);
    s_npc_facing = false;

    /* L4 — icone notif (em cima da recepcionista, fica acima do NPC pelo
     * z-order de criacao) */
    s_icone = lv_image_create(s_root);
    lv_image_set_src(s_icone, &img_rec_icone_notif);
    lv_obj_set_pos(s_icone, IMG_REC_ICONE_NOTIF_META.off_x,
                            IMG_REC_ICONE_NOTIF_META.off_y);
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
    lv_image_set_src(s_dlg_box, &img_rec_caixa_dialogo);
    lv_obj_set_pos(s_dlg_box, IMG_REC_CAIXA_DIALOGO_META.off_x,
                              IMG_REC_CAIXA_DIALOGO_META.off_y);
    no_scroll(s_dlg_box);
    lv_obj_add_flag(s_dlg_box, LV_OBJ_FLAG_HIDDEN);

    /* Label do texto: dentro da regiao da CAIXA_DELIMITADORA_TEXTO_08
     * (156, 32, 231, 74). Margem interna pequena. */
    s_dlg_text = lv_label_create(s_root);
    lv_label_set_text(s_dlg_text, "");
    lv_label_set_long_mode(s_dlg_text, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(s_dlg_text, IMG_REC_CAIXA_TEXTO_META.w - 8);
    lv_obj_set_pos(s_dlg_text, IMG_REC_CAIXA_TEXTO_META.off_x + 4,
                               IMG_REC_CAIXA_TEXTO_META.off_y + 4);
    lv_obj_set_style_text_color(s_dlg_text, lv_color_white(), LV_PART_MAIN);
    no_scroll(s_dlg_text);
    lv_obj_add_flag(s_dlg_text, LV_OBJ_FLAG_HIDDEN);

    /* Hint "[A] >>  [B] Pular" no canto inf-direito da caixa */
    s_dlg_hint = lv_label_create(s_root);
    lv_label_set_text(s_dlg_hint, "[A] >>  [B] Pular");
    lv_obj_set_style_text_color(s_dlg_hint, lv_color_hex(0xFFA500), LV_PART_MAIN);
    lv_obj_set_pos(s_dlg_hint,
                   IMG_REC_CAIXA_DIALOGO_META.off_x + IMG_REC_CAIXA_DIALOGO_META.w - 130,
                   IMG_REC_CAIXA_DIALOGO_META.off_y + IMG_REC_CAIXA_DIALOGO_META.h - 18);
    no_scroll(s_dlg_hint);
    lv_obj_add_flag(s_dlg_hint, LV_OBJ_FLAG_HIDDEN);

    s_dlg_state = DLG_INACTIVE;
    s_dlg_played = false;
    s_dlg_line = 0;
    s_dlg_char = 0;

    s_timer = lv_timer_create(recepcao_tick, UI_TICK_MS, NULL);
    ESP_LOGI(TAG, "recepcao built (player @ %d,%d)", s_px, s_py);
}

void screen_recepcao_destroy(void)
{
    if (s_timer) { lv_timer_delete(s_timer); s_timer = NULL; }
    if (s_root)  {
        lv_obj_delete(s_root);
        s_root = NULL;
        s_player = s_npc = s_icone = s_prompt = NULL;
        s_dlg_box = s_dlg_text = s_dlg_hint = NULL;
    }
}
