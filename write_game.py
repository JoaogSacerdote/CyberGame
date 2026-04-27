#!/usr/bin/env python3
"""Writes the new cybersec_game.c v1 Real implementation."""

CODE = r"""/**
 * =============================================================================
 * CyberSec: Network Defender — LVGL PC Simulator  v1 Real
 * =============================================================================
 *
 * TELAS IMPLEMENTADAS:
 *  - Tela Inicial (CYBERSEC / PRESSIONE A PARA INICIAR)
 *  - Tutorial com NPC (8 passos via dialogo com portrait)
 *  - Sala Principal (Recepcao) - layout pixel-art com moveis
 *  - Sala 2 (Escritorio) - acessivel pela porta lateral
 *  - HUD overlay: relogio, barra de integridade, nome da sala, status
 *  - Eventos flutuantes coloridos (verde=?, amarelo=!, vermelho=X)
 *  - Dialog detalhado de vulnerabilidade com detalhes e barra HP
 *  - Game Over / Vitoria
 *
 * CONTROLES:
 *  [Setas]    Mover personagem
 *  [A/SPACE]  Confirmar / Resolver tarefa verde ou amarela
 *  [N]        NFC scan (resolver ransomware vermelho)
 *  [R]        Reiniciar
 * =============================================================================
 */

#include "cybersec_game.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* SDL key codes */
#ifndef SDLK_UP
#define SDLK_UP         0x40000052u
#define SDLK_DOWN       0x40000051u
#define SDLK_LEFT       0x40000050u
#define SDLK_RIGHT      0x4000004Fu
#define SDLK_SPACE      0x00000020u
#define SDLK_RETURN     0x0000000Du
#define SDLK_a          0x00000061u
#define SDLK_A          0x00000041u
#define SDLK_n          0x0000006Eu
#define SDLK_N          0x0000004Eu
#define SDLK_r          0x00000072u
#define SDLK_R          0x00000052u
#define SDLK_ESCAPE     0x0000001Bu
#endif

/* ============================================================
 *  CONSTANTES
 * ============================================================ */
#define SCR_W           480
#define SCR_H           320
#define HUD_H           30
#define GAME_Y          HUD_H
#define GAME_H          (SCR_H - HUD_H)   /* 290 */

#define PLR_W           12
#define PLR_H           18
#define PLR_SPEED       2
#define MOVE_MS         16

/* Relogio: 3 min reais = 10h de expediente */
#define CLOCK_TICK_MS       500
#define GAME_TOTAL_TICKS    360
#define GAME_TOTAL_MINUTES  600

/* Eventos */
#define MAX_EVENTS      4
#define EVENT_MIN_MS    9000
#define EVENT_MAX_MS    16000
#define DRAIN_TICK_MS   2000
#define INTERACT_RADIUS 50

/* Pontos quentes de eventos — Sala 1 */
#define S1_SPOTS 4
static const int S1_X[S1_SPOTS] = { 68, 185, 320, 405 };
static const int S1_Y[S1_SPOTS] = { 185, 240, 195, 240 };

/* Pontos quentes de eventos — Sala 2 */
#define S2_SPOTS 4
static const int S2_X[S2_SPOTS] = { 96, 212, 312, 425 };
static const int S2_Y[S2_SPOTS] = { 156, 196, 172, 216 };

/* ============================================================
 *  TIPOS
 * ============================================================ */
typedef enum { GS_TITLE = 0, GS_TUTORIAL, GS_PLAYING, GS_GAMEOVER } GameState;
typedef enum { EV_NONE = 0, EV_ROUTINE, EV_ANOMALY, EV_CRITICAL }   EventType;
typedef enum { ROOM_1 = 0, ROOM_2 = 1 }                              RoomID;

typedef struct {
    bool      active;
    EventType type;
    int       spot;
    RoomID    room;
    int       hp;
    char      victim[28];
    char      detail[44];
    lv_obj_t *bubble;
    lv_obj_t *bubble_lbl;
} GameEvent;

typedef struct { int x, y; } Player;
typedef struct { bool up, down, left, right; } KeyState;

/* ============================================================
 *  ESTADO GLOBAL
 * ============================================================ */
static GameState  g_state         = GS_TITLE;
static RoomID     g_room          = ROOM_1;
static bool       g_game_running  = false;
static int        g_integrity     = 100;
static int        g_game_hour     = 8;
static int        g_game_min      = 0;
static int        g_clock_ticks   = 0;
static bool       g_nfc_ready     = true;
static Player     g_plr           = { 240, 190 };
static KeyState   g_keys          = { false, false, false, false };
static GameEvent  g_events[MAX_EVENTS];
static int        g_tutorial_step = 0;
static int        g_score         = 0;

/* ============================================================
 *  OBJETOS LVGL
 * ============================================================ */
static lv_obj_t  *g_hud_cont      = NULL;
static lv_obj_t  *g_lbl_time      = NULL;
static lv_obj_t  *g_bar_hp        = NULL;
static lv_obj_t  *g_lbl_hp_val    = NULL;
static lv_obj_t  *g_lbl_status    = NULL;
static lv_obj_t  *g_lbl_room_name = NULL;
static lv_obj_t  *g_room_cont     = NULL;
static lv_obj_t  *g_player_body   = NULL;
static lv_obj_t  *g_player_head   = NULL;
static lv_obj_t  *g_dlg_overlay   = NULL;

static lv_timer_t *g_tmr_move     = NULL;
static lv_timer_t *g_tmr_clock    = NULL;
static lv_timer_t *g_tmr_event    = NULL;
static lv_timer_t *g_tmr_drain    = NULL;

/* ============================================================
 *  DADOS DO TUTORIAL
 * ============================================================ */
#define TUTORIAL_STEPS 8

typedef struct {
    uint32_t hair;
    uint32_t shirt;
    const char *text;
} TutLine;

static const TutLine k_tut[TUTORIAL_STEPS] = {
    { 0x3A2010u, 0x1E3A6Eu,
      "Bom dia! Voce deve ser o novo\nanalista de ciberseguranca\ncontratado, correto? Bem-vindo\na empresa." },
    { 0x3A2010u, 0x1E3A6Eu,
      "Seu trabalho e proteger a rede\nda empresa ate as 18h.\nUse as SETAS para mover." },
    { 0x3A2010u, 0x1E3A6Eu,
      "As tarefas VERDES sao de\nprevencao. Sao opcionais e\ntranquilas. Icone: ?" },
    { 0x3A2010u, 0x1E3A6Eu,
      "Os alertas AMARELOS sao\nanomalias suspeitas. Resolva\nantes que causem danos. Icone: !" },
    { 0x3A2010u, 0x1E3A6Eu,
      "As tarefas VERMELHAS sao\nataques ativos (Ransomware).\nAja imediatamente! Icone: X" },
    { 0x3A2010u, 0x1E3A6Eu,
      "Aproxime-se do icone colorido\ne pressione [A] para resolver.\nRansomware exige [N] para NFC." },
    { 0x3A2010u, 0x1E3A6Eu,
      "Se um incidente for ignorado,\na integridade da rede cai.\nChegar a ZERO = Game Over!" },
    { 0x3A2010u, 0x1E3A6Eu,
      "Depois passe pela porta e\nverifique se ha mais\nocorrencias pelo andar. Boa sorte!" },
};

static const char *k_victims[] = {
    "Joao Silva", "Maria Souza", "Carlos Lima",
    "Ana Costa",  "Pedro Rocha", "Lucia Alves"
};
static const char *k_det_r[] = {
    "Atualizacao pendente",
    "Backup nao realizado",
    "Patch disponivel",
    "Certificado expirando"
};
static const char *k_det_a[] = {
    "Senha fraca: Admin1",
    "Login suspeito 03:12",
    "Tentativa de phishing",
    "Acesso nao autorizado"
};
static const char *k_det_c[] = {
    "Criptografia iniciada!",
    "Arquivos bloqueados",
    "Malware na rede",
    "C&C detectado"
};

/* ============================================================
 *  FORWARD DECLARATIONS
 * ============================================================ */
static void create_title_screen(void);
static void create_hud(void);
static void build_room1(void);
static void build_room2(void);
static void create_player(void);

static void start_timers(void);
static void stop_timers(void);
static void switch_room(RoomID room);
static int  nearest_event(void);
static void spawn_event_slot(int slot);
static void resolve_event(int slot, bool nfc);
static void recalc_integrity(void);
static void do_game_over(bool victory);
static void do_restart(void);

static void update_hud(void);
static void update_player_pos(void);
static void rebuild_event_bubbles(void);
static void set_status(const char *msg, lv_color_t c);

static void show_tutorial_step(int step);
static void show_event_dialog(int slot);
static void close_dialog(bool advance_tut);

static void tmr_move_cb (lv_timer_t *t);
static void tmr_clock_cb(lv_timer_t *t);
static void tmr_event_cb(lv_timer_t *t);
static void tmr_drain_cb(lv_timer_t *t);

static lv_obj_t *mk_rect(lv_obj_t *p, int x, int y, int w, int h, uint32_t col);
static lv_obj_t *mk_rect_r(lv_obj_t *p, int x, int y, int w, int h,
                             uint32_t col, int radius);
static void draw_npc(lv_obj_t *p, int x, int y,
                     uint32_t hair, uint32_t shirt, bool sitting);
static void draw_portrait(lv_obj_t *p, int x, int y, int sz,
                          uint32_t hair, uint32_t shirt);
static lv_color_t  ev_color(EventType t);
static const char *ev_label(EventType t);
static const char *ev_icon (EventType t);

/* ============================================================
 *  API PUBLICA
 * ============================================================ */

void cybersec_start(void)
{
    srand((unsigned int)time(NULL));

    g_state        = GS_TITLE;
    g_room         = ROOM_1;
    g_game_running = false;
    g_integrity    = 100;
    g_game_hour    = 8;
    g_game_min     = 0;
    g_clock_ticks  = 0;
    g_nfc_ready    = true;
    g_plr.x        = 240;
    g_plr.y        = 190;
    g_score        = 0;
    g_tutorial_step = 0;
    g_dlg_overlay  = NULL;
    g_hud_cont     = NULL;
    g_room_cont    = NULL;
    g_player_body  = NULL;
    g_player_head  = NULL;
    memset(&g_keys,   0, sizeof(g_keys));
    memset(g_events,  0, sizeof(g_events));

    lv_obj_clean(lv_scr_act());
    lv_obj_set_style_bg_color(lv_scr_act(), lv_color_hex(0x080F1A), 0);

    create_title_screen();
}

void cybersec_sdl_key_event(int32_t sdlk, bool is_down)
{
    /* Movimento continuo */
    if ((uint32_t)sdlk == SDLK_UP)    { g_keys.up    = is_down; return; }
    if ((uint32_t)sdlk == SDLK_DOWN)  { g_keys.down  = is_down; return; }
    if ((uint32_t)sdlk == SDLK_LEFT)  { g_keys.left  = is_down; return; }
    if ((uint32_t)sdlk == SDLK_RIGHT) { g_keys.right = is_down; return; }

    if (!is_down) return;

    /* Reiniciar — sempre disponivel */
    if ((uint32_t)sdlk == SDLK_r || (uint32_t)sdlk == SDLK_R) {
        do_restart();
        return;
    }

    /* Tela inicial */
    if (g_state == GS_TITLE) {
        if ((uint32_t)sdlk == SDLK_a     || (uint32_t)sdlk == SDLK_A ||
            (uint32_t)sdlk == SDLK_SPACE || (uint32_t)sdlk == SDLK_RETURN) {
            lv_obj_clean(lv_scr_act());
            lv_obj_set_style_bg_color(lv_scr_act(), lv_color_hex(0x080F1A), 0);
            create_hud();
            build_room1();
            create_player();
            update_hud();
            g_state = GS_TUTORIAL;
            g_tutorial_step = 0;
            show_tutorial_step(0);
        }
        return;
    }

    /* Tutorial */
    if (g_state == GS_TUTORIAL) {
        if ((uint32_t)sdlk == SDLK_a     || (uint32_t)sdlk == SDLK_A ||
            (uint32_t)sdlk == SDLK_SPACE || (uint32_t)sdlk == SDLK_RETURN) {
            close_dialog(true);
        }
        return;
    }

    /* Dialog de evento aberto */
    if (g_dlg_overlay != NULL) {
        bool is_nfc = ((uint32_t)sdlk == SDLK_n || (uint32_t)sdlk == SDLK_N);
        bool is_act = ((uint32_t)sdlk == SDLK_a || (uint32_t)sdlk == SDLK_A ||
                       (uint32_t)sdlk == SDLK_SPACE || (uint32_t)sdlk == SDLK_RETURN);

        if (!is_act && !is_nfc) return;

        int slot = nearest_event();
        if (slot >= 0) {
            EventType t = g_events[slot].type;
            if (t == EV_CRITICAL && !is_nfc) {
                close_dialog(false);
                set_status("Ransomware! Use [N] para NFC scan.", lv_color_hex(0xFF3333));
            } else if (t == EV_CRITICAL && is_nfc) {
                close_dialog(false);
                resolve_event(slot, true);
            } else {
                close_dialog(false);
                resolve_event(slot, false);
            }
        } else {
            close_dialog(false);
        }
        return;
    }

    if (!g_game_running) return;

    /* Interacao com evento */
    if ((uint32_t)sdlk == SDLK_a || (uint32_t)sdlk == SDLK_A ||
        (uint32_t)sdlk == SDLK_SPACE || (uint32_t)sdlk == SDLK_RETURN) {
        int slot = nearest_event();
        if (slot < 0) {
            set_status("Nenhum incidente por aqui.", lv_color_hex(0x888888));
        } else {
            show_event_dialog(slot);
        }
        return;
    }

    /* NFC scan */
    if ((uint32_t)sdlk == SDLK_n || (uint32_t)sdlk == SDLK_N) {
        int slot = nearest_event();
        if (slot < 0 || g_events[slot].type != EV_CRITICAL) {
            set_status("Sem Ransomware nas proximidades.", lv_color_hex(0x888888));
        } else if (!g_nfc_ready) {
            set_status("Cartao NFC nao detectado!", lv_color_hex(0xFF3333));
        } else {
            show_event_dialog(slot);
        }
        return;
    }
}

/* ============================================================
 *  TELA INICIAL
 * ============================================================ */
static void create_title_screen(void)
{
    /* Fundo — sala escurecida */
    lv_obj_t *bg = lv_obj_create(lv_scr_act());
    lv_obj_set_size(bg, SCR_W, SCR_H);
    lv_obj_set_pos(bg, 0, 0);
    lv_obj_set_style_bg_color(bg, lv_color_hex(0x0D1A2E), 0);
    lv_obj_set_style_border_width(bg, 0, 0);
    lv_obj_set_style_pad_all(bg, 0, 0);
    lv_obj_clear_flag(bg, LV_OBJ_FLAG_SCROLLABLE);

    /* Silhueta da sala (elementos escurecidos) */
    mk_rect(bg, 0,   0,   SCR_W, 75,  0x181E30);  /* parede */
    mk_rect(bg, 0,   75,  SCR_W, 2,   0x101828);  /* divisor */
    mk_rect(bg, 0,   77,  SCR_W, SCR_H - 77, 0x141C2C); /* chao */

    mk_rect(bg, 22,  12,  72, 56, 0x201808);  /* prateleira */
    mk_rect(bg, 162, 10,  28, 54, 0x2A1A08);  /* porta */
    mk_rect(bg, 222, 12,  70, 42, 0x202030);  /* quadro branco */
    mk_rect(bg, 104, 95, 250, 112, 0x1A0808); /* tapete borda */
    mk_rect(bg, 120, 110, 215, 82, 0x200E0E);
    mk_rect(bg, 152, 124, 148, 55, 0x201808);

    mk_rect(bg, 4,   212, 100, 58, 0x201408); /* mesa esq */
    mk_rect(bg, 344, 210, 102, 48, 0x201808); /* mesa dir */
    mk_rect(bg, 310, 238, 88,  44, 0x181828); /* sofa */

    /* Overlay escuro */
    lv_obj_t *ov = lv_obj_create(lv_scr_act());
    lv_obj_set_size(ov, SCR_W, SCR_H);
    lv_obj_set_pos(ov, 0, 0);
    lv_obj_set_style_bg_color(ov, lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_opa(ov, LV_OPA_70, 0);
    lv_obj_set_style_border_width(ov, 0, 0);
    lv_obj_clear_flag(ov, LV_OBJ_FLAG_SCROLLABLE);

    /* === TITULO === */
    lv_obj_t *lbl_t = lv_label_create(lv_scr_act());
    lv_label_set_text(lbl_t, "CYBERSEC");
    lv_obj_set_style_text_color(lbl_t, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_text_font(lbl_t, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_letter_space(lbl_t, 10, 0);
    lv_obj_align(lbl_t, LV_ALIGN_CENTER, 0, -28);

    /* Linha azul */
    lv_obj_t *sep = lv_obj_create(lv_scr_act());
    lv_obj_set_size(sep, 260, 2);
    lv_obj_set_style_bg_color(sep, lv_color_hex(0x1A44DD), 0);
    lv_obj_set_style_border_width(sep, 0, 0);
    lv_obj_align(sep, LV_ALIGN_CENTER, 0, 0);
    lv_obj_clear_flag(sep, LV_OBJ_FLAG_SCROLLABLE);

    /* Subtitulo */
    lv_obj_t *lbl_s = lv_label_create(lv_scr_act());
    lv_label_set_text(lbl_s, "PRESSIONE A PARA INICIAR");
    lv_obj_set_style_text_color(lbl_s, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_text_font(lbl_s, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_letter_space(lbl_s, 4, 0);
    lv_obj_align(lbl_s, LV_ALIGN_CENTER, 0, 26);
}

/* ============================================================
 *  HUD
 * ============================================================ */
static void create_hud(void)
{
    g_hud_cont = lv_obj_create(lv_scr_act());
    lv_obj_set_size(g_hud_cont, SCR_W, HUD_H);
    lv_obj_set_pos(g_hud_cont, 0, 0);
    lv_obj_set_style_bg_color(g_hud_cont, lv_color_hex(0x080F1A), 0);
    lv_obj_set_style_border_color(g_hud_cont, lv_color_hex(0x00CFAA), 0);
    lv_obj_set_style_border_width(g_hud_cont, 0, 0);
    lv_obj_set_style_border_side(g_hud_cont, LV_BORDER_SIDE_BOTTOM, 0);
    lv_obj_set_style_border_width(g_hud_cont, 1, 0);
    lv_obj_set_style_pad_all(g_hud_cont, 0, 0);
    lv_obj_clear_flag(g_hud_cont, LV_OBJ_FLAG_SCROLLABLE);

    g_lbl_time = lv_label_create(g_hud_cont);
    lv_label_set_text(g_lbl_time, "08:00");
    lv_obj_set_style_text_color(g_lbl_time, lv_color_hex(0x00CFAA), 0);
    lv_obj_set_style_text_font(g_lbl_time, &lv_font_montserrat_14, 0);
    lv_obj_align(g_lbl_time, LV_ALIGN_LEFT_MID, 6, 0);

    g_lbl_room_name = lv_label_create(g_hud_cont);
    lv_label_set_text(g_lbl_room_name, "Recepcao");
    lv_obj_set_style_text_color(g_lbl_room_name, lv_color_hex(0x4488AA), 0);
    lv_obj_set_style_text_font(g_lbl_room_name, &lv_font_montserrat_10, 0);
    lv_obj_align(g_lbl_room_name, LV_ALIGN_LEFT_MID, 74, 0);

    lv_obj_t *lr = lv_label_create(g_hud_cont);
    lv_label_set_text(lr, "REDE:");
    lv_obj_set_style_text_color(lr, lv_color_hex(0x3A6688), 0);
    lv_obj_set_style_text_font(lr, &lv_font_montserrat_10, 0);
    lv_obj_align(lr, LV_ALIGN_LEFT_MID, 158, 0);

    g_bar_hp = lv_bar_create(g_hud_cont);
    lv_obj_set_size(g_bar_hp, 100, 10);
    lv_obj_align(g_bar_hp, LV_ALIGN_LEFT_MID, 198, 0);
    lv_bar_set_range(g_bar_hp, 0, 100);
    lv_bar_set_value(g_bar_hp, 100, LV_ANIM_OFF);
    lv_obj_set_style_bg_color(g_bar_hp, lv_color_hex(0x1C2E40), 0);
    lv_obj_set_style_bg_color(g_bar_hp, lv_color_hex(0x00FF88), LV_PART_INDICATOR);
    lv_obj_set_style_radius(g_bar_hp, 2, 0);
    lv_obj_set_style_radius(g_bar_hp, 2, LV_PART_INDICATOR);

    g_lbl_hp_val = lv_label_create(g_hud_cont);
    lv_label_set_text(g_lbl_hp_val, "100%");
    lv_obj_set_style_text_color(g_lbl_hp_val, lv_color_hex(0x88FFCC), 0);
    lv_obj_set_style_text_font(g_lbl_hp_val, &lv_font_montserrat_10, 0);
    lv_obj_align(g_lbl_hp_val, LV_ALIGN_LEFT_MID, 303, 0);

    g_lbl_status = lv_label_create(g_hud_cont);
    lv_label_set_text(g_lbl_status, "SISTEMA OK");
    lv_obj_set_style_text_color(g_lbl_status, lv_color_hex(0x44CC88), 0);
    lv_obj_set_style_text_font(g_lbl_status, &lv_font_montserrat_10, 0);
    lv_obj_set_width(g_lbl_status, 132);
    lv_label_set_long_mode(g_lbl_status, LV_LABEL_LONG_CLIP);
    lv_obj_set_style_text_align(g_lbl_status, LV_TEXT_ALIGN_RIGHT, 0);
    lv_obj_align(g_lbl_status, LV_ALIGN_RIGHT_MID, -4, 0);
}

/* ============================================================
 *  SALA 1 — Recepcao
 * ============================================================ */
static void build_room1(void)
{
    if (g_room_cont) { lv_obj_del(g_room_cont); g_room_cont = NULL; }

    g_room_cont = lv_obj_create(lv_scr_act());
    lv_obj_set_size(g_room_cont, SCR_W, GAME_H);
    lv_obj_set_pos(g_room_cont, 0, GAME_Y);
    lv_obj_set_style_bg_color(g_room_cont, lv_color_hex(0x909090), 0);
    lv_obj_set_style_border_width(g_room_cont, 0, 0);
    lv_obj_set_style_pad_all(g_room_cont, 0, 0);
    lv_obj_clear_flag(g_room_cont, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *p = g_room_cont;

    /* == PAREDE TOPO == */
    mk_rect(p, 0, 0, SCR_W, 72, 0xC0C0C0);
    mk_rect(p, 0, 72, SCR_W, 2,  0x888888);

    /* == CHAO — grid sutil == */
    mk_rect(p, 0, 74, SCR_W, GAME_H - 74, 0x909090);
    for (int c = 0; c < SCR_W; c += 32)
        mk_rect(p, c, 74, 1, GAME_H - 74, 0x7C7C7C);
    for (int r = 74; r < GAME_H; r += 32)
        mk_rect(p, 0, r, SCR_W, 1, 0x7C7C7C);

    /* == MOVEIS PAREDE == */

    /* Planta esquerda */
    mk_rect(p,  4,  52, 16, 18, 0x4A5A28);
    mk_rect(p,  6,  42, 12, 12, 0x2A6A20);
    mk_rect(p,  2,  36,  8,  8, 0x1E5018);
    mk_rect(p, 12,  34, 10, 10, 0x22581C);

    /* Prateleira */
    mk_rect(p, 24,  10, 72, 58, 0x6B4A2A);
    mk_rect(p, 26,  12, 68,  4, 0x8B6A3A);
    mk_rect(p, 26,  24, 68,  4, 0x8B6A3A);
    mk_rect(p, 26,  36, 68,  4, 0x8B6A3A);
    mk_rect(p, 26,  48, 68,  4, 0x8B6A3A);
    mk_rect(p, 28,  17, 10,  7, 0x3A7A44);
    mk_rect(p, 42,  15,  8,  9, 0x4A3AA0);
    mk_rect(p, 54,  16, 14,  8, 0xA04A3A);
    mk_rect(p, 28,  28,  8,  8, 0x8A8A3A);
    mk_rect(p, 40,  27,  6,  9, 0x3A8AAA);
    mk_rect(p, 52,  29, 12,  7, 0x7A3A8A);
    mk_rect(p, 28,  40, 14,  8, 0x5A7A3A);
    mk_rect(p, 46,  40, 10,  8, 0xAA5A3A);

    /* Porta */
    mk_rect(p, 162, 10, 30, 56, 0x5C3B1E);
    mk_rect(p, 164, 11, 26, 54, 0x8B6032);
    mk_rect(p, 164, 11, 26,  2, 0x6B4820);
    mk_rect(p, 175, 20,  3,  3, 0x2A1A08);
    mk_rect(p, 162, 64, 30,  8, 0x686868);

    /* Quadro branco */
    mk_rect(p, 224, 12, 72, 44, 0x404050);
    mk_rect(p, 226, 14, 68, 40, 0xD0D0D8);
    mk_rect(p, 230, 18, 20,  3, 0x888898);
    mk_rect(p, 230, 25, 30,  3, 0x888898);
    mk_rect(p, 230, 32, 24,  3, 0x888898);
    mk_rect(p, 228, 54, 68,  3, 0x606070);

    /* Quadro foto */
    mk_rect(p, 306, 12, 38, 34, 0x504040);
    mk_rect(p, 308, 14, 34, 30, 0x80A860);
    mk_rect(p, 312, 18, 10,  8, 0xF0CC60);

    /* Rack servidor */
    mk_rect(p, 370, 10, 18, 60, 0x404050);
    mk_rect(p, 372, 13, 14,  4, 0x202030);
    mk_rect(p, 372, 19, 14,  4, 0x202030);
    mk_rect(p, 372, 25, 14,  4, 0x202030);
    mk_rect(p, 372, 31, 14,  4, 0x202030);
    mk_rect(p, 374, 14,  4,  2, 0x00FF44);
    mk_rect(p, 374, 20,  4,  2, 0x0044FF);

    /* Bebedouro */
    mk_rect(p, 396, 15, 20, 30, 0x8090A8);
    mk_rect(p, 398, 11, 16,  8, 0xC0D0E0);
    mk_rect(p, 400, 13, 12,  6, 0xA8BCCC);
    mk_rect(p, 400, 44,  8,  6, 0x607080);

    /* Ar condicionado */
    mk_rect(p, 430,  4, 46, 18, 0xC0C8D0);
    mk_rect(p, 432,  6, 42, 14, 0xD8E0E8);
    mk_rect(p, 434,  8, 38,  3, 0xB0B8C0);
    mk_rect(p, 434, 13, 38,  2, 0xB0B8C0);

    /* == TAPETE CENTRAL == */
    mk_rect(p, 104,  96, 248, 118, 0x7A2020);
    mk_rect(p, 118, 110, 218,  90, 0xBB5020);
    mk_rect(p, 150, 124, 152,  62, 0xC89838);
    mk_rect(p, 168, 134, 116,  44, 0xD8AA50);

    /* == AREA ESQUERDA — Mesa chefe == */
    mk_rect(p,   4, 212, 102, 58, 0x8B6A32);
    mk_rect(p,   4, 212, 102,  4, 0x6B4A20);
    mk_rect(p,   4, 266, 102,  4, 0x6B4A20);
    mk_rect(p,  10, 196,  26, 18, 0x202020);
    mk_rect(p,  12, 198,  22, 14, 0x1A3A6A);
    mk_rect(p,  15, 201,  16,  8, 0x2A5A9A);
    mk_rect(p,  22, 214,   6,  2, 0x383838);
    mk_rect(p,  38, 196,  26, 18, 0x202020);
    mk_rect(p,  40, 198,  22, 14, 0x1A3A6A);
    mk_rect(p,  43, 201,  16,  8, 0x2A5A9A);
    mk_rect(p,  50, 214,   6,  2, 0x383838);
    mk_rect(p,  12, 220,  50,  8, 0x303038);
    mk_rect(p,  14, 221,  46,  6, 0x404048);
    mk_rect_r(p,65, 220,  12, 10, 0x282830, 3);
    /* Cadeira */
    mk_rect(p,  42, 248,  34, 20, 0x2A2A38);
    mk_rect(p,  44, 238,  30, 12, 0x222230);
    mk_rect(p,  40, 266,   8,  8, 0x1C1C28);
    mk_rect(p,  68, 266,   8,  8, 0x1C1C28);
    /* NPC chefe sentado */
    draw_npc(p, 54, 234, 0x1A1A1A, 0x2A2A3A, true);
    /* Impressora */
    mk_rect(p,   4, 240,  28, 22, 0x484858);
    mk_rect(p,   6, 242,  24,  8, 0x383848);
    mk_rect(p,   6, 252,  24,  4, 0x585868);

    /* == AREA DIREITA — Recepcao == */
    mk_rect(p, 346, 214, 100, 46, 0x9A7840);
    mk_rect(p, 346, 214, 100,  4, 0x7A5820);
    mk_rect(p, 346, 256, 100,  4, 0x7A5820);
    /* Globo */
    mk_rect_r(p, 360, 202, 22, 22, 0x4070CC, 11);
    mk_rect(p, 366, 196,  10,  8, 0x8090D0);
    mk_rect(p, 370, 224,   4,  4, 0x604020);
    /* PC */
    mk_rect(p, 406, 200,  32, 22, 0x282830);
    mk_rect(p, 408, 202,  28, 18, 0x1A3060);
    mk_rect(p, 410, 204,  24, 14, 0x2A4080);
    mk_rect(p, 418, 222,   8,  2, 0x383840);
    /* Livro */
    mk_rect(p, 384, 210,  20, 26, 0xA86430);
    mk_rect(p, 386, 212,  16, 22, 0xD09050);
    mk_rect(p, 388, 215,  12,  2, 0x806030);
    /* Sofa */
    mk_rect(p, 310, 240,  88, 40, 0x6878A0);
    mk_rect(p, 310, 240,  88,  8, 0x78889A);
    mk_rect(p, 310, 240,   8, 40, 0x78889A);
    mk_rect(p, 390, 240,   8, 40, 0x78889A);
    mk_rect(p, 318, 268,  72, 12, 0x505A7A);
    /* NPC recepcao */
    draw_npc(p, 440, 222, 0x8B5A28, 0x1E4A8E, false);
    /* Planta dir */
    mk_rect(p, 452, 248,  18, 24, 0x4A5A28);
    mk_rect(p, 454, 238,  14, 12, 0x2A6A20);
    mk_rect(p, 450, 232,  10, 10, 0x1E5018);

    /* == PORTA SALA 2 (parede direita) == */
    mk_rect(p, 468, 125, 12, 56, 0x787878);
    mk_rect(p, 470, 127,  8, 52, 0x909090);

    lv_obj_t *arrow = lv_label_create(p);
    lv_label_set_text(arrow, "► 2");
    lv_obj_set_style_text_color(arrow, lv_color_hex(0x00FFAA), 0);
    lv_obj_set_style_text_font(arrow, &lv_font_montserrat_10, 0);
    lv_obj_set_pos(arrow, 456, 148);
}

/* ============================================================
 *  SALA 2 — Escritorio
 * ============================================================ */
static void build_room2(void)
{
    if (g_room_cont) { lv_obj_del(g_room_cont); g_room_cont = NULL; }

    g_room_cont = lv_obj_create(lv_scr_act());
    lv_obj_set_size(g_room_cont, SCR_W, GAME_H);
    lv_obj_set_pos(g_room_cont, 0, GAME_Y);
    lv_obj_set_style_bg_color(g_room_cont, lv_color_hex(0x909090), 0);
    lv_obj_set_style_border_width(g_room_cont, 0, 0);
    lv_obj_set_style_pad_all(g_room_cont, 0, 0);
    lv_obj_clear_flag(g_room_cont, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *p = g_room_cont;

    /* Chao */
    mk_rect(p, 0, 0, SCR_W, GAME_H, 0x909090);
    for (int c = 0; c < SCR_W; c += 32)
        mk_rect(p, c, 0, 1, GAME_H, 0x7C7C7C);
    for (int r = 0; r < GAME_H; r += 32)
        mk_rect(p, 0, r, SCR_W, 1, 0x7C7C7C);

    /* Parede topo */
    mk_rect(p, 0, 0, SCR_W, 62, 0xC0C0C0);
    mk_rect(p, 0, 62, SCR_W, 2,  0x888888);

    /* Parede lateral esq (corredor) */
    mk_rect(p, 0, 0, 22, GAME_H, 0xB0B0B0);
    mk_rect(p, 20, 0,  2, GAME_H, 0x808080);
    mk_rect(p, 0, 100, 22, 80,   0x888888);  /* abertura */

    /* Seta de voltar */
    lv_obj_t *back = lv_label_create(p);
    lv_label_set_text(back, "◄ 1");
    lv_obj_set_style_text_color(back, lv_color_hex(0x00FFAA), 0);
    lv_obj_set_style_text_font(back, &lv_font_montserrat_10, 0);
    lv_obj_set_pos(back, 1, 136);

    /* == FILEIRA 1 DE MESAS (y=72) == */
    /* Mesa A */
    mk_rect(p,  60,  72, 88, 52, 0x8B7040);
    mk_rect(p,  60,  72, 88,  4, 0x6B5020);
    mk_rect(p,  66,  62, 22, 14, 0x202028);
    mk_rect(p,  68,  64, 18, 10, 0x1A3060);
    mk_rect(p,  74,  72,  6,  2, 0x303038);
    mk_rect(p,  68,  86, 46,  6, 0x383840);
    mk_rect(p,  72, 114, 28, 16, 0x303040);
    mk_rect(p,  74, 110, 24,  8, 0x282838);
    draw_npc(p,  80, 102, 0xA08050, 0x2A6050, true);

    /* Mesa B */
    mk_rect(p, 172,  72, 88, 52, 0x8B7040);
    mk_rect(p, 172,  72, 88,  4, 0x6B5020);
    mk_rect(p, 178,  62, 22, 14, 0x202028);
    mk_rect(p, 180,  64, 18, 10, 0x1A3060);
    mk_rect(p, 186,  72,  6,  2, 0x303038);
    mk_rect(p, 180,  86, 46,  6, 0x383840);
    mk_rect(p, 184, 114, 28, 16, 0x303040);
    mk_rect(p, 186, 110, 24,  8, 0x282838);
    draw_npc(p, 190, 102, 0x3A7040, 0x6A3A80, true);

    /* Mesa C */
    mk_rect(p, 284,  72, 88, 52, 0x8B7040);
    mk_rect(p, 284,  72, 88,  4, 0x6B5020);
    mk_rect(p, 290,  62, 22, 14, 0x202028);
    mk_rect(p, 292,  64, 18, 10, 0x1A3060);
    mk_rect(p, 298,  72,  6,  2, 0x303038);
    mk_rect(p, 292,  86, 46,  6, 0x383840);
    mk_rect(p, 296, 114, 28, 16, 0x303040);
    mk_rect(p, 298, 110, 24,  8, 0x282838);
    draw_npc(p, 302, 102, 0x604030, 0x4A4A80, true);

    /* Mesa D */
    mk_rect(p, 392,  72, 80, 52, 0x8B7040);
    mk_rect(p, 392,  72, 80,  4, 0x6B5020);
    mk_rect(p, 396,  62, 22, 14, 0x202028);
    mk_rect(p, 398,  64, 18, 10, 0x1A3060);
    mk_rect(p, 404,  72,  6,  2, 0x303038);
    mk_rect(p, 398,  86, 40,  6, 0x383840);
    mk_rect(p, 396, 114, 28, 16, 0x303040);
    mk_rect(p, 398, 110, 24,  8, 0x282838);

    /* == FILEIRA 2 DE MESAS (y=168) == */
    /* Mesa E */
    mk_rect(p,  60, 168, 88, 52, 0x8B7040);
    mk_rect(p,  60, 168, 88,  4, 0x6B5020);
    mk_rect(p,  66, 158, 22, 14, 0x202028);
    mk_rect(p,  68, 160, 18, 10, 0x1A3060);
    mk_rect(p,  74, 168,  6,  2, 0x303038);
    mk_rect(p,  68, 182, 46,  6, 0x383840);
    mk_rect(p,  72, 210, 28, 16, 0x303040);
    mk_rect(p,  74, 206, 24,  8, 0x282838);
    draw_npc(p,  80, 198, 0x8A5030, 0x805030, true);

    /* Mesa F */
    mk_rect(p, 172, 168, 88, 52, 0x8B7040);
    mk_rect(p, 172, 168, 88,  4, 0x6B5020);
    mk_rect(p, 178, 158, 22, 14, 0x202028);
    mk_rect(p, 180, 160, 18, 10, 0x1A3060);
    mk_rect(p, 186, 168,  6,  2, 0x303038);
    mk_rect(p, 180, 182, 46,  6, 0x383840);
    mk_rect(p, 184, 210, 28, 16, 0x303040);
    mk_rect(p, 186, 206, 24,  8, 0x282838);

    /* Impressoras */
    mk_rect(p, 282, 170, 36, 24, 0x484858);
    mk_rect(p, 284, 172, 32,  8, 0x383848);
    mk_rect(p, 284, 182, 32,  4, 0x585868);
    mk_rect(p, 332, 170, 36, 24, 0x484858);
    mk_rect(p, 334, 172, 32,  8, 0x383848);
    mk_rect(p, 334, 182, 32,  4, 0x585868);

    /* Plantas */
    mk_rect(p, 440, 168, 18, 24, 0x4A5A28);
    mk_rect(p, 442, 158, 14, 12, 0x2A6A20);
    mk_rect(p, 438, 152, 10, 10, 0x1E5018);
    mk_rect(p, 440, 238, 18, 24, 0x4A5A28);
    mk_rect(p, 442, 228, 14, 12, 0x2A6A20);

    /* Bebedouro */
    mk_rect(p, 400, 170, 20, 30, 0x8090A8);
    mk_rect(p, 402, 166, 16,  8, 0xC0D0E0);
    mk_rect(p, 402, 198,  8,  6, 0x607080);

    /* NPC no corredor */
    draw_npc(p, 338, 234, 0x3A7040, 0x1E3A6E, false);

    /* Corredor fundo */
    mk_rect(p, 22, 270, SCR_W - 22, 20, 0x888888);
    mk_rect(p, 22, 268, SCR_W - 22,  2, 0x686868);
}

/* ============================================================
 *  PERSONAGEM
 * ============================================================ */
static void create_player(void)
{
    g_player_head = lv_obj_create(g_room_cont);
    lv_obj_set_size(g_player_head, PLR_W, 8);
    lv_obj_set_style_bg_color(g_player_head, lv_color_hex(0xE8B896), 0);
    lv_obj_set_style_border_color(g_player_head, lv_color_hex(0xB89060), 0);
    lv_obj_set_style_border_width(g_player_head, 1, 0);
    lv_obj_set_style_radius(g_player_head, 3, 0);
    lv_obj_set_style_pad_all(g_player_head, 0, 0);
    lv_obj_clear_flag(g_player_head, LV_OBJ_FLAG_SCROLLABLE);

    /* Cabelo marrom */
    lv_obj_t *hair = lv_obj_create(g_player_head);
    lv_obj_set_size(hair, PLR_W - 2, 4);
    lv_obj_set_pos(hair, 1, 0);
    lv_obj_set_style_bg_color(hair, lv_color_hex(0x8B5020), 0);
    lv_obj_set_style_border_width(hair, 0, 0);
    lv_obj_set_style_radius(hair, 2, 0);
    lv_obj_set_style_pad_all(hair, 0, 0);
    lv_obj_clear_flag(hair, LV_OBJ_FLAG_SCROLLABLE);

    g_player_body = lv_obj_create(g_room_cont);
    lv_obj_set_size(g_player_body, PLR_W, 10);
    lv_obj_set_style_bg_color(g_player_body, lv_color_hex(0x1E4A9E), 0);
    lv_obj_set_style_border_width(g_player_body, 0, 0);
    lv_obj_set_style_radius(g_player_body, 1, 0);
    lv_obj_set_style_pad_all(g_player_body, 0, 0);
    lv_obj_clear_flag(g_player_body, LV_OBJ_FLAG_SCROLLABLE);

    update_player_pos();
}

/* ============================================================
 *  TIMERS
 * ============================================================ */
static void start_timers(void)
{
    if (!g_tmr_move)  g_tmr_move  = lv_timer_create(tmr_move_cb,  MOVE_MS,        NULL);
    if (!g_tmr_clock) g_tmr_clock = lv_timer_create(tmr_clock_cb, CLOCK_TICK_MS,  NULL);
    if (!g_tmr_event) g_tmr_event = lv_timer_create(tmr_event_cb, EVENT_MIN_MS,   NULL);
    if (!g_tmr_drain) g_tmr_drain = lv_timer_create(tmr_drain_cb, DRAIN_TICK_MS,  NULL);
}

static void stop_timers(void)
{
    if (g_tmr_move)  { lv_timer_del(g_tmr_move);  g_tmr_move  = NULL; }
    if (g_tmr_clock) { lv_timer_del(g_tmr_clock); g_tmr_clock = NULL; }
    if (g_tmr_event) { lv_timer_del(g_tmr_event); g_tmr_event = NULL; }
    if (g_tmr_drain) { lv_timer_del(g_tmr_drain); g_tmr_drain = NULL; }
}

static void tmr_move_cb(lv_timer_t *t)
{
    (void)t;
    if (!g_game_running || g_dlg_overlay) return;

    /* Transicao sala 1 -> sala 2 */
    if (g_room == ROOM_1 &&
        g_plr.x >= SCR_W - PLR_W - 14 &&
        g_plr.y > GAME_Y + 118 && g_plr.y < GAME_Y + 186) {
        switch_room(ROOM_2);
        return;
    }
    /* Transicao sala 2 -> sala 1 */
    if (g_room == ROOM_2 &&
        g_plr.x <= 26 &&
        g_plr.y > GAME_Y + 96 && g_plr.y < GAME_Y + 185) {
        switch_room(ROOM_1);
        return;
    }

    if (g_keys.up)    g_plr.y -= PLR_SPEED;
    if (g_keys.down)  g_plr.y += PLR_SPEED;
    if (g_keys.left)  g_plr.x -= PLR_SPEED;
    if (g_keys.right) g_plr.x += PLR_SPEED;

    int min_x = (g_room == ROOM_2) ? 24 : 0;
    if (g_plr.x < min_x)           g_plr.x = min_x;
    if (g_plr.x > SCR_W - PLR_W)   g_plr.x = SCR_W - PLR_W;
    if (g_plr.y < GAME_Y + 2)      g_plr.y = GAME_Y + 2;
    if (g_plr.y > SCR_H - PLR_H)   g_plr.y = SCR_H - PLR_H;

    update_player_pos();
}

static void tmr_clock_cb(lv_timer_t *t)
{
    (void)t;
    if (!g_game_running) return;

    g_clock_ticks++;
    int total_min = (g_clock_ticks * GAME_TOTAL_MINUTES) / GAME_TOTAL_TICKS;
    g_game_hour   = 8  + (total_min / 60);
    g_game_min    = total_min % 60;
    update_hud();

    if (g_game_hour >= 18) do_game_over(true);
}

static void tmr_event_cb(lv_timer_t *t)
{
    if (!g_game_running || g_dlg_overlay) return;

    int slot = -1;
    for (int i = 0; i < MAX_EVENTS; i++) {
        if (!g_events[i].active) { slot = i; break; }
    }
    if (slot < 0) return;

    spawn_event_slot(slot);

    uint32_t next = (uint32_t)(EVENT_MIN_MS + rand() % (EVENT_MAX_MS - EVENT_MIN_MS));
    lv_timer_set_period(t, next);
}

static void tmr_drain_cb(lv_timer_t *t)
{
    (void)t;
    if (!g_game_running) return;

    for (int i = 0; i < MAX_EVENTS; i++) {
        if (!g_events[i].active) continue;
        int d = (g_events[i].type == EV_ROUTINE) ? 1 :
                (g_events[i].type == EV_ANOMALY) ? 4 : 10;
        g_events[i].hp -= d;
        if (g_events[i].hp < 0) g_events[i].hp = 0;
    }

    recalc_integrity();
    update_hud();

    if (g_integrity <= 0) do_game_over(false);
}

/* ============================================================
 *  LOGICA DE JOGO
 * ============================================================ */
static void switch_room(RoomID room)
{
    g_room = room;
    memset(&g_keys, 0, sizeof(g_keys));

    if (room == ROOM_1) {
        g_plr.x = SCR_W - PLR_W - 22;
        g_plr.y = GAME_Y + 152;
        build_room1();
        lv_label_set_text(g_lbl_room_name, "Recepcao");
        set_status("Sala Principal.", lv_color_hex(0x44CC88));
    } else {
        g_plr.x = 30;
        g_plr.y = GAME_Y + 152;
        build_room2();
        lv_label_set_text(g_lbl_room_name, "Escritorio");
        set_status("Escritorio - Andar 2.", lv_color_hex(0x44CC88));
    }

    create_player();
    rebuild_event_bubbles();
}

static void spawn_event_slot(int slot)
{
    GameEvent *ev = &g_events[slot];
    ev->active = true;
    ev->hp     = 100;
    ev->room   = (rand() % 2 == 0) ? ROOM_1 : ROOM_2;

    int ns   = (ev->room == ROOM_1) ? S1_SPOTS : S2_SPOTS;
    ev->spot = rand() % ns;

    int r = rand() % 10;
    ev->type = (r < 4) ? EV_ROUTINE : (r < 7) ? EV_ANOMALY : EV_CRITICAL;

    snprintf(ev->victim, sizeof(ev->victim), "%s", k_victims[rand() % 6]);

    const char **det = (ev->type == EV_ROUTINE)  ? k_det_r :
                       (ev->type == EV_ANOMALY)   ? k_det_a : k_det_c;
    snprintf(ev->detail, sizeof(ev->detail), "%s", det[rand() % 4]);

    rebuild_event_bubbles();

    char buf[72];
    snprintf(buf, sizeof(buf), "ALERTA [%s] em %s!",
             ev_label(ev->type),
             (ev->room == ROOM_1) ? "Recepcao" : "Escritorio");
    set_status(buf, ev_color(ev->type));

    if (ev->type == EV_CRITICAL)
        lv_obj_set_style_bg_color(g_hud_cont, lv_color_hex(0x280808), 0);
}

static void resolve_event(int slot, bool nfc)
{
    if (!g_events[slot].active) return;

    if (g_events[slot].bubble) {
        lv_obj_del(g_events[slot].bubble);
        g_events[slot].bubble    = NULL;
        g_events[slot].bubble_lbl = NULL;
    }
    g_events[slot].active = false;
    g_events[slot].type   = EV_NONE;

    g_score += nfc ? 30 : 15;
    recalc_integrity();
    update_hud();

    char buf[72];
    snprintf(buf, sizeof(buf), "Resolvido! +%d pts  Integridade: %d%%",
             nfc ? 30 : 15, g_integrity);
    set_status(buf, lv_color_hex(0x44FF88));
    lv_obj_set_style_bg_color(g_hud_cont, lv_color_hex(0x080F1A), 0);
}

static int nearest_event(void)
{
    int best = -1, best_d = INTERACT_RADIUS * INTERACT_RADIUS;

    for (int i = 0; i < MAX_EVENTS; i++) {
        if (!g_events[i].active || g_events[i].room != g_room) continue;

        const int *xs = (g_room == ROOM_1) ? S1_X : S2_X;
        const int *ys = (g_room == ROOM_1) ? S1_Y : S2_Y;
        int ex = xs[g_events[i].spot];
        int ey = ys[g_events[i].spot] + GAME_Y;

        int dx = g_plr.x + PLR_W / 2 - ex;
        int dy = g_plr.y + PLR_H / 2 - ey;
        int d2 = dx * dx + dy * dy;
        if (d2 < best_d) { best_d = d2; best = i; }
    }
    return best;
}

static void recalc_integrity(void)
{
    int total = 0, cnt = 0;
    for (int i = 0; i < MAX_EVENTS; i++) {
        if (g_events[i].active) { total += g_events[i].hp; cnt++; }
    }
    if (cnt == 0) {
        g_integrity = 100;
    } else {
        g_integrity = 60 + (total / cnt) * 40 / 100;
        if (g_integrity > 100) g_integrity = 100;
        if (g_integrity < 0)   g_integrity = 0;
    }
}

static void do_game_over(bool victory)
{
    g_game_running = false;
    stop_timers();
    memset(&g_keys, 0, sizeof(g_keys));

    char msg[220];
    if (victory) {
        snprintf(msg, sizeof(msg),
                 "Expediente encerrado com sucesso!\n\n"
                 "Integridade final: %d%%\n"
                 "Pontuacao: %d pontos\n\n"
                 "Voce protegeu a rede da empresa!\n\n"
                 "Pressione [R] para jogar novamente.",
                 g_integrity, g_score);
    } else {
        snprintf(msg, sizeof(msg),
                 "FALHA CRITICA NO SISTEMA!\n\n"
                 "A integridade chegou a ZERO.\n"
                 "A empresa foi comprometida.\n\n"
                 "Pontuacao: %d pontos\n\n"
                 "Pressione [R] para tentar novamente.",
                 g_score);
    }

    if (g_dlg_overlay) { lv_obj_del(g_dlg_overlay); g_dlg_overlay = NULL; }

    g_dlg_overlay = lv_obj_create(lv_scr_act());
    lv_obj_set_size(g_dlg_overlay, SCR_W, SCR_H);
    lv_obj_set_pos(g_dlg_overlay, 0, 0);
    lv_obj_set_style_bg_color(g_dlg_overlay, lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_opa(g_dlg_overlay, LV_OPA_80, 0);
    lv_obj_set_style_border_width(g_dlg_overlay, 0, 0);
    lv_obj_clear_flag(g_dlg_overlay, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *box = lv_obj_create(g_dlg_overlay);
    lv_obj_set_size(box, 320, 200);
    lv_obj_center(box);
    lv_obj_set_style_bg_color(box, lv_color_hex(0x080F1A), 0);
    lv_obj_set_style_border_color(box,
        victory ? lv_color_hex(0x00FFAA) : lv_color_hex(0xFF3333), 0);
    lv_obj_set_style_border_width(box, 2, 0);
    lv_obj_set_style_radius(box, 6, 0);
    lv_obj_set_style_pad_all(box, 12, 0);
    lv_obj_clear_flag(box, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *ttl = lv_label_create(box);
    lv_label_set_text(ttl, victory ? "MISSAO CONCLUIDA!" : "GAME OVER");
    lv_obj_set_style_text_color(ttl,
        victory ? lv_color_hex(0x00FFAA) : lv_color_hex(0xFF3333), 0);
    lv_obj_set_style_text_font(ttl, &lv_font_montserrat_14, 0);
    lv_obj_align(ttl, LV_ALIGN_TOP_MID, 0, 0);

    lv_obj_t *sep = lv_obj_create(box);
    lv_obj_set_size(sep, 290, 1);
    lv_obj_set_style_bg_color(sep, lv_color_hex(0x1E3A50), 0);
    lv_obj_set_style_border_width(sep, 0, 0);
    lv_obj_align(sep, LV_ALIGN_TOP_MID, 0, 22);
    lv_obj_clear_flag(sep, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *m = lv_label_create(box);
    lv_label_set_text(m, msg);
    lv_obj_set_style_text_color(m, lv_color_hex(0xAABBCC), 0);
    lv_obj_set_style_text_font(m, &lv_font_montserrat_10, 0);
    lv_obj_set_width(m, 290);
    lv_label_set_long_mode(m, LV_LABEL_LONG_WRAP);
    lv_obj_align(m, LV_ALIGN_TOP_MID, 0, 28);
}

static void do_restart(void)
{
    stop_timers();
    memset(&g_keys, 0, sizeof(g_keys));
    cybersec_start();
}

/* ============================================================
 *  DIALOGOS
 * ============================================================ */
static void show_tutorial_step(int step)
{
    if (step >= TUTORIAL_STEPS) {
        if (g_dlg_overlay) { lv_obj_del(g_dlg_overlay); g_dlg_overlay = NULL; }
        g_state        = GS_PLAYING;
        g_game_running = true;
        start_timers();
        set_status("Boa sorte, Analista!", lv_color_hex(0x00FFAA));
        return;
    }

    if (g_dlg_overlay) { lv_obj_del(g_dlg_overlay); g_dlg_overlay = NULL; }

    const TutLine *line = &k_tut[step];

    /* Overlay semi-transparente */
    g_dlg_overlay = lv_obj_create(lv_scr_act());
    lv_obj_set_size(g_dlg_overlay, SCR_W, SCR_H);
    lv_obj_set_pos(g_dlg_overlay, 0, 0);
    lv_obj_set_style_bg_color(g_dlg_overlay, lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_opa(g_dlg_overlay, LV_OPA_40, 0);
    lv_obj_set_style_border_width(g_dlg_overlay, 0, 0);
    lv_obj_clear_flag(g_dlg_overlay, LV_OBJ_FLAG_SCROLLABLE);

    /* Caixa de dialogo inferior */
    lv_obj_t *box = lv_obj_create(g_dlg_overlay);
    lv_obj_set_size(box, SCR_W - 10, 140);
    lv_obj_set_pos(box, 5, SCR_H - 148);
    lv_obj_set_style_bg_color(box, lv_color_hex(0x030608), 0);
    lv_obj_set_style_bg_opa(box, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(box, lv_color_hex(0x1E3050), 0);
    lv_obj_set_style_border_width(box, 2, 0);
    lv_obj_set_style_radius(box, 4, 0);
    lv_obj_set_style_pad_all(box, 0, 0);
    lv_obj_clear_flag(box, LV_OBJ_FLAG_SCROLLABLE);

    /* Frame do portrait */
    lv_obj_t *pframe = lv_obj_create(box);
    lv_obj_set_size(pframe, 78, 124);
    lv_obj_set_pos(pframe, 6, 8);
    lv_obj_set_style_bg_color(pframe, lv_color_hex(0x000000), 0);
    lv_obj_set_style_border_color(pframe, lv_color_hex(0x1E3050), 0);
    lv_obj_set_style_border_width(pframe, 2, 0);
    lv_obj_set_style_radius(pframe, 2, 0);
    lv_obj_set_style_pad_all(pframe, 0, 0);
    lv_obj_clear_flag(pframe, LV_OBJ_FLAG_SCROLLABLE);

    draw_portrait(pframe, 8, 6, 62, line->hair, line->shirt);

    /* Texto */
    lv_obj_t *txt = lv_label_create(box);
    lv_label_set_text(txt, line->text);
    lv_obj_set_style_text_color(txt, lv_color_hex(0xDDEEFF), 0);
    lv_obj_set_style_text_font(txt, &lv_font_montserrat_14, 0);
    lv_obj_set_size(txt, SCR_W - 115, 104);
    lv_obj_set_pos(txt, 90, 8);
    lv_label_set_long_mode(txt, LV_LABEL_LONG_WRAP);

    /* Indicador de continuacao */
    lv_obj_t *cont = lv_label_create(box);
    char cbuf[28];
    snprintf(cbuf, sizeof(cbuf), "[A]  %d/%d  ►", step + 1, TUTORIAL_STEPS);
    lv_label_set_text(cont, cbuf);
    lv_obj_set_style_text_color(cont, lv_color_hex(0x5588AA), 0);
    lv_obj_set_style_text_font(cont, &lv_font_montserrat_10, 0);
    lv_obj_align(cont, LV_ALIGN_BOTTOM_RIGHT, -6, -4);
}

static void show_event_dialog(int slot)
{
    if (g_dlg_overlay) { lv_obj_del(g_dlg_overlay); g_dlg_overlay = NULL; }

    GameEvent *ev = &g_events[slot];

    g_dlg_overlay = lv_obj_create(lv_scr_act());
    lv_obj_set_size(g_dlg_overlay, SCR_W, SCR_H);
    lv_obj_set_pos(g_dlg_overlay, 0, 0);
    lv_obj_set_style_bg_color(g_dlg_overlay, lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_opa(g_dlg_overlay, LV_OPA_60, 0);
    lv_obj_set_style_border_width(g_dlg_overlay, 0, 0);
    lv_obj_clear_flag(g_dlg_overlay, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *box = lv_obj_create(g_dlg_overlay);
    lv_obj_set_size(box, 308, 218);
    lv_obj_center(box);
    lv_obj_set_style_bg_color(box, lv_color_hex(0x03060A), 0);
    lv_obj_set_style_bg_opa(box, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(box, ev_color(ev->type), 0);
    lv_obj_set_style_border_width(box, 2, 0);
    lv_obj_set_style_radius(box, 4, 0);
    lv_obj_set_style_pad_all(box, 12, 0);
    lv_obj_clear_flag(box, LV_OBJ_FLAG_SCROLLABLE);

    /* Titulo */
    char ttl[80];
    snprintf(ttl, sizeof(ttl), "%s  %s",
             ev_icon(ev->type),
             ev->type == EV_CRITICAL ? "RANSOMWARE DETECTADO!" :
             ev->type == EV_ANOMALY  ? "VULNERABILIDADE DETECTADA" :
                                       "TAREFA DE PREVENCAO");

    lv_obj_t *t = lv_label_create(box);
    lv_label_set_text(t, ttl);
    lv_obj_set_style_text_color(t, ev_color(ev->type), 0);
    lv_obj_set_style_text_font(t, &lv_font_montserrat_10, 0);
    lv_obj_set_width(t, 280);
    lv_label_set_long_mode(t, LV_LABEL_LONG_WRAP);
    lv_obj_align(t, LV_ALIGN_TOP_MID, 0, 0);

    /* Separador */
    lv_obj_t *sep = lv_obj_create(box);
    lv_obj_set_size(sep, 280, 1);
    lv_obj_set_style_bg_color(sep, ev_color(ev->type), 0);
    lv_obj_set_style_border_width(sep, 0, 0);
    lv_obj_align(sep, LV_ALIGN_TOP_MID, 0, 20);
    lv_obj_clear_flag(sep, LV_OBJ_FLAG_SCROLLABLE);

    /* Detalhes */
    char det[200];
    snprintf(det, sizeof(det),
             "* Sala:        %s\n"
             "* Usuario:     %s\n"
             "* Detalhe:     %s\n"
             "* Integridade: %d%%\n"
             "* Nivel risco: %s",
             (ev->room == ROOM_1) ? "Recepcao" : "Escritorio",
             ev->victim,
             ev->detail,
             ev->hp,
             ev->type == EV_CRITICAL ? "CRITICO" :
             ev->type == EV_ANOMALY  ? "Alto" : "Baixo");

    lv_obj_t *d = lv_label_create(box);
    lv_label_set_text(d, det);
    lv_obj_set_style_text_color(d, lv_color_hex(0xCCDDEE), 0);
    lv_obj_set_style_text_font(d, &lv_font_montserrat_10, 0);
    lv_obj_set_width(d, 280);
    lv_label_set_long_mode(d, LV_LABEL_LONG_WRAP);
    lv_obj_align(d, LV_ALIGN_TOP_MID, 0, 28);

    /* Barra HP do evento */
    lv_obj_t *hbar = lv_bar_create(box);
    lv_obj_set_size(hbar, 280, 8);
    lv_obj_align(hbar, LV_ALIGN_BOTTOM_MID, 0, -20);
    lv_bar_set_range(hbar, 0, 100);
    lv_bar_set_value(hbar, ev->hp, LV_ANIM_OFF);
    lv_obj_set_style_bg_color(hbar, lv_color_hex(0x0E1E2E), 0);
    lv_obj_set_style_bg_color(hbar, ev_color(ev->type), LV_PART_INDICATOR);
    lv_obj_set_style_radius(hbar, 2, 0);
    lv_obj_set_style_radius(hbar, 2, LV_PART_INDICATOR);

    /* Instrucao */
    const char *act =
        (ev->type == EV_CRITICAL) ? "[ N = NFC scan para resolver ]" :
                                    "[ A = Corrigir vulnerabilidade ]";
    lv_obj_t *ai = lv_label_create(box);
    lv_label_set_text(ai, act);
    lv_obj_set_style_text_color(ai, ev_color(ev->type), 0);
    lv_obj_set_style_text_font(ai, &lv_font_montserrat_10, 0);
    lv_obj_align(ai, LV_ALIGN_BOTTOM_MID, 0, -4);
}

static void close_dialog(bool advance_tut)
{
    if (g_dlg_overlay) { lv_obj_del(g_dlg_overlay); g_dlg_overlay = NULL; }

    if (g_state == GS_TUTORIAL && advance_tut) {
        g_tutorial_step++;
        show_tutorial_step(g_tutorial_step);
    }
}

/* ============================================================
 *  ATUALIZACAO DE UI
 * ============================================================ */
static void update_hud(void)
{
    if (!g_lbl_time) return;

    char buf[10];
    snprintf(buf, sizeof(buf), "%02d:%02d", g_game_hour, g_game_min);
    lv_label_set_text(g_lbl_time, buf);

    lv_bar_set_value(g_bar_hp, g_integrity, LV_ANIM_ON);

    char pct[8];
    snprintf(pct, sizeof(pct), "%d%%", g_integrity);
    lv_label_set_text(g_lbl_hp_val, pct);

    lv_color_t c =
        (g_integrity > 60) ? lv_color_hex(0x00FF88) :
        (g_integrity > 30) ? lv_color_hex(0xFFCC00) : lv_color_hex(0xFF4444);
    lv_obj_set_style_bg_color(g_bar_hp, c, LV_PART_INDICATOR);
    lv_obj_set_style_text_color(g_lbl_hp_val, c, 0);
}

static void update_player_pos(void)
{
    if (!g_player_head || !g_player_body) return;
    int ry = g_plr.y - GAME_Y;
    lv_obj_set_pos(g_player_head, g_plr.x, ry);
    lv_obj_set_pos(g_player_body, g_plr.x, ry + 8);
}

static void rebuild_event_bubbles(void)
{
    for (int i = 0; i < MAX_EVENTS; i++) {
        if (g_events[i].bubble) {
            lv_obj_del(g_events[i].bubble);
            g_events[i].bubble    = NULL;
            g_events[i].bubble_lbl = NULL;
        }
        if (!g_events[i].active || g_events[i].room != g_room) continue;

        const int *xs = (g_room == ROOM_1) ? S1_X : S2_X;
        const int *ys = (g_room == ROOM_1) ? S1_Y : S2_Y;
        int bx = xs[g_events[i].spot] - 14;
        int by = ys[g_events[i].spot] - 32;

        lv_obj_t *bub = lv_obj_create(g_room_cont);
        lv_obj_set_size(bub, 28, 28);
        lv_obj_set_pos(bub, bx, by);
        lv_obj_set_style_bg_color(bub, ev_color(g_events[i].type), 0);
        lv_obj_set_style_border_color(bub, lv_color_hex(0xFFFFFF), 0);
        lv_obj_set_style_border_width(bub, 2, 0);
        lv_obj_set_style_radius(bub, 14, 0);
        lv_obj_set_style_pad_all(bub, 0, 0);
        lv_obj_clear_flag(bub, LV_OBJ_FLAG_SCROLLABLE);

        lv_obj_t *lbl = lv_label_create(bub);
        lv_label_set_text(lbl, ev_icon(g_events[i].type));
        lv_obj_set_style_text_color(lbl, lv_color_hex(0xFFFFFF), 0);
        lv_obj_set_style_text_font(lbl, &lv_font_montserrat_14, 0);
        lv_obj_center(lbl);

        g_events[i].bubble    = bub;
        g_events[i].bubble_lbl = lbl;
    }
}

static void set_status(const char *msg, lv_color_t c)
{
    if (!g_lbl_status) return;
    lv_label_set_text(g_lbl_status, msg);
    lv_obj_set_style_text_color(g_lbl_status, c, 0);
}

/* ============================================================
 *  HELPERS DE DESENHO
 * ============================================================ */
static lv_obj_t *mk_rect(lv_obj_t *p, int x, int y, int w, int h, uint32_t col)
{
    lv_obj_t *o = lv_obj_create(p);
    lv_obj_set_size(o, w, h);
    lv_obj_set_pos(o, x, y);
    lv_obj_set_style_bg_color(o, lv_color_hex(col), 0);
    lv_obj_set_style_border_width(o, 0, 0);
    lv_obj_set_style_radius(o, 0, 0);
    lv_obj_set_style_pad_all(o, 0, 0);
    lv_obj_clear_flag(o, LV_OBJ_FLAG_SCROLLABLE);
    return o;
}

static lv_obj_t *mk_rect_r(lv_obj_t *p, int x, int y, int w, int h,
                             uint32_t col, int radius)
{
    lv_obj_t *o = mk_rect(p, x, y, w, h, col);
    lv_obj_set_style_radius(o, radius, 0);
    return o;
}

static void draw_npc(lv_obj_t *p, int x, int y,
                     uint32_t hair, uint32_t shirt, bool sitting)
{
    mk_rect(p, x,      y,     10, 8, 0xE0B090);
    mk_rect(p, x,      y,     10, 4, hair);
    mk_rect(p, x + 2,  y + 5,  2, 2, 0x282020);
    mk_rect(p, x + 6,  y + 5,  2, 2, 0x282020);

    if (!sitting) {
        mk_rect(p, x + 1,  y + 8,  8, 8, shirt);
        mk_rect(p, x,      y + 8,  3, 8, shirt);
        mk_rect(p, x + 7,  y + 8,  3, 8, shirt);
        mk_rect(p, x + 2,  y + 16, 3, 6, 0x2A2A40);
        mk_rect(p, x + 5,  y + 16, 3, 6, 0x2A2A40);
    } else {
        mk_rect(p, x + 1,  y + 8,  8, 6, shirt);
        mk_rect(p, x - 2,  y + 12, 14, 4, 0x2A2A40);
    }
}

static void draw_portrait(lv_obj_t *p, int x, int y, int sz,
                          uint32_t hair, uint32_t shirt)
{
    int hw = sz * 10 / 20;
    int hh = sz * 9 / 20;
    int hx = x + (sz - hw) / 2;
    int hy = y + 4;

    mk_rect(p, x, y, sz, sz, 0x080C14);

    /* Corpo */
    mk_rect(p, x + 4,        hy + hh + 6, sz - 8, sz - hh - 8, shirt);

    /* Rosto */
    mk_rect(p, hx,            hy,          hw,      hh,           0xE0B890);
    /* Cabelo */
    mk_rect(p, hx,            hy,          hw,      hh * 4 / 10, hair);
    /* Olhos */
    mk_rect(p, hx + 3,        hy + hh / 2 + 1, hw / 5, hh / 6, 0x202020);
    mk_rect(p, hx + hw - 6,   hy + hh / 2 + 1, hw / 5, hh / 6, 0x202020);
    /* Boca */
    mk_rect(p, hx + 4,        hy + hh * 3 / 4, hw - 8, 2, 0x884040);
    /* Nariz */
    mk_rect(p, hx + hw/2 - 1, hy + hh * 3 / 5, 2, 2, 0xC08060);
    /* Orelhas */
    mk_rect(p, hx - 2,        hy + hh / 3, 3, hh / 4, 0xD0A870);
    mk_rect(p, hx + hw - 1,   hy + hh / 3, 3, hh / 4, 0xD0A870);
}

/* ============================================================
 *  HELPERS DE TIPO
 * ============================================================ */
static lv_color_t ev_color(EventType t)
{
    switch (t) {
        case EV_ROUTINE:  return lv_color_hex(0x00BB44);
        case EV_ANOMALY:  return lv_color_hex(0xFFAA00);
        case EV_CRITICAL: return lv_color_hex(0xFF2222);
        default:          return lv_color_hex(0x2A4060);
    }
}

static const char *ev_label(EventType t)
{
    switch (t) {
        case EV_ROUTINE:  return "TAREFA";
        case EV_ANOMALY:  return "ANOMALIA";
        case EV_CRITICAL: return "RANSOMWARE";
        default:          return "";
    }
}

static const char *ev_icon(EventType t)
{
    switch (t) {
        case EV_ROUTINE:  return "?";
        case EV_ANOMALY:  return "!";
        case EV_CRITICAL: return "X";
        default:          return " ";
    }
}
"""

path = r'c:\Users\silva\Downloads\AV3-Cyber\simulation\lv_port_pc_eclipse\cybersec_game.c'
with open(path, 'w', encoding='utf-8') as f:
    f.write(CODE)

print(f"Written {len(CODE)} chars, {CODE.count(chr(10))} lines")
