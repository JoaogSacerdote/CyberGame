/**
 * =============================================================================
 * CyberSec: Network Defender — LVGL PC Simulator  v2 Sprites
 * =============================================================================
 *
 * TELAS:
 *  - Tela Inicial  : imagem tela_inicial.png
 *  - Sala 1 (Recepcao): Tela01.png + secretario.png (inferior esq.)
 *  - Sala 2 (Escritorios): Tela02.png + npc_sala2.png + trabalhador_sala2.png
 *
 * SPRITES personagem (48x128, 12x16 por frame):
 *   Linha  0 = andar para baixo  (4 frames)
 *   Linha 16 = andar para esquerda
 *   Linha 32 = andar para direita
 *   Linha 48 = andar para cima
 *   ... etc (ate 8 linhas / 128px)
 *
 * NPC sprites (384x32): 24 colunas de 16x32 — frame 0 (sentado/parado)
 *
 * FLUXO:
 *  1. Tela inicial -> pressiona A
 *  2. Sala 1: ir ate a secretaria (inferior esq.) e pressionar A -> tutorial
 *  3. Jogador vai para Sala 2 pela porta direita
 *  4. Interagir com trabalhador Carlos (inferior direito) -> primeira task
 *  5. Resolver task (identificar senha fraca) -> jogo principal inicia
 *
 * CONTROLES:
 *  [Setas]   Mover
 *  [A/SPACE] Interagir / Confirmar dialogo
 *  [N]       NFC scan (ransomware)
 *  [R]       Reiniciar
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
#define SCR_W            480
#define SCR_H            320
#define HUD_H            30
#define GAME_Y           HUD_H
#define GAME_H           (SCR_H - HUD_H)   /* 290 */

/* Caminhos das imagens (relativo ao diretorio de execucao bin/) */
#define IMG_TITLE        "A:img/tela_inicial.png"
#define IMG_SALA1        "A:img/Tela01.png"
#define IMG_SALA2        "A:img/Tela02.png"
#define IMG_PLAYER       "A:img/persoagem.png"
#define IMG_SECRETARIO   "A:img/secretario.png"
#define IMG_NPC_SALA2    "A:img/npc_sala2.png"
#define IMG_TRABALHADOR  "A:img/trabalhador_sala2.png"

/* Sprite do personagem: folha 48x128, 3 frames x 4 direcoes (16x32 cada) */
#define PLR_FRAME_W      16
#define PLR_FRAME_H      32
#define PLR_FRAMES       3
#define PLR_DIR_DOWN     0
#define PLR_DIR_LEFT     1
#define PLR_DIR_RIGHT    2
#define PLR_DIR_UP       3

/* NPC sprites: folha 384x32, cada frame 16x32 */
#define NPC_FRAME_W      16
#define NPC_FRAME_H      32

#define PLR_W            PLR_FRAME_W
#define PLR_H            PLR_FRAME_H
#define PLR_SPEED        2
#define MOVE_MS          16
#define ANIM_MS          150

/* Relogio */
#define CLOCK_TICK_MS        500
#define GAME_TOTAL_TICKS     360
#define GAME_TOTAL_MINUTES   600

/* Eventos */
#define MAX_EVENTS       4
#define EVENT_MIN_MS     9000
#define EVENT_MAX_MS     16000
#define DRAIN_TICK_MS    2000
#define INTERACT_RADIUS  55

/* Pontos quentes — Sala 1 */
#define S1_SPOTS 4
static const int S1_X[S1_SPOTS] = { 68, 185, 320, 405 };
static const int S1_Y[S1_SPOTS] = { 185, 240, 195, 240 };

/* Pontos quentes — Sala 2 */
#define S2_SPOTS 4
static const int S2_X[S2_SPOTS] = { 96, 212, 312, 425 };
static const int S2_Y[S2_SPOTS] = { 156, 196, 172, 216 };

/* Posicao da secretaria na Sala 1 (cadeira inferior esquerda)
 * - centro visual: (SEC_X, SEC_Y - NPC_FRAME_H)
 * - clip topo-esq: (SEC_X - NPC_FRAME_W, SEC_Y - NPC_FRAME_H*2) */
#define SEC_X   90
#define SEC_Y   280

/* Posicao do trabalhador na Sala 2 (estacao inferior direita)
 * - centro visual: (WORKER_X + NPC_FRAME_W, WORKER_Y + NPC_FRAME_H)
 * - clip topo-esq: (WORKER_X, WORKER_Y) */
#define WORKER_X  320
#define WORKER_Y  55

/* Raio de interacao com NPCs */
#define NPC_INTERACT_R   55

/* ============================================================
 *  TIPOS
 * ============================================================ */
typedef enum {
    GS_TITLE = 0,
    GS_SECRETARIA,
    GS_WORKER_INTRO,
    GS_WORKER_TASK,
    GS_PLAYING,
    GS_GAMEOVER
} GameState;

typedef enum { EV_NONE = 0, EV_ROUTINE, EV_ANOMALY, EV_CRITICAL } EventType;
typedef enum { ROOM_1 = 0, ROOM_2 = 1 }                           RoomID;

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
static GameState  g_state            = GS_TITLE;
static RoomID     g_room             = ROOM_1;
static bool       g_game_running     = false;
static int        g_integrity        = 100;
static int        g_game_hour        = 8;
static int        g_game_min         = 0;
static int        g_clock_ticks      = 0;
static bool       g_nfc_ready        = true;
static Player     g_plr              = { 60, GAME_Y + 200 };
static KeyState   g_keys             = { false, false, false, false };
static GameEvent  g_events[MAX_EVENTS];
static int        g_sec_step         = 0;
static int        g_worker_step      = 0;
static int        g_score            = 0;
static bool       g_worker_task_done = false;
static bool       g_secretaria_done  = false;
static int        g_plr_frame        = 0;
static int        g_plr_dir          = PLR_DIR_DOWN;
static bool       g_plr_moving       = false;
static int        g_task_selected    = 0;

/* ============================================================
 *  OBJETOS LVGL
 * ============================================================ */
static lv_obj_t  *g_hud_cont        = NULL;
static lv_obj_t  *g_lbl_time        = NULL;
static lv_obj_t  *g_bar_hp          = NULL;
static lv_obj_t  *g_lbl_hp_val      = NULL;
static lv_obj_t  *g_lbl_status      = NULL;
static lv_obj_t  *g_lbl_room_name   = NULL;
static lv_obj_t  *g_room_cont       = NULL;
static lv_obj_t  *g_player_img      = NULL;
static lv_obj_t  *g_dlg_overlay     = NULL;

static lv_timer_t *g_tmr_move       = NULL;
static lv_timer_t *g_tmr_clock      = NULL;
static lv_timer_t *g_tmr_event      = NULL;
static lv_timer_t *g_tmr_drain      = NULL;
static lv_timer_t *g_tmr_anim       = NULL;

/* Container de clipping do personagem (pai de g_player_img) */
static lv_obj_t  *g_player_cont     = NULL;

/* ============================================================
 *  DIALOGOS DA SECRETARIA
 * ============================================================ */
#define SEC_STEPS 7
static const char *k_sec_dialog[SEC_STEPS] = {
    "Ola! Voce deve ser o novo analista\nde ciberseguranca. Bem-vindo a\nCyberCorp! Eu sou a Ana.",
    "Seu objetivo e proteger a rede\nda empresa durante o expediente,\nde 08h ate as 18h.",
    "Use as SETAS para mover pelo\nandar. Va a sala de escritorios\npela porta na parede direita.",
    "Fique atento a alertas coloridos:\n  ? VERDE = Tarefa de prevencao\n  ! AMARELO = Anomalia suspeita\n  X VERMELHO = Ransomware ativo!",
    "Aproxime-se do icone e pressione\n[A] para resolver. Ransomware\nexige [N] para scan NFC.",
    "Se um incidente for ignorado,\na integridade da rede cai.\nChegar a ZERO = Game Over!",
    "Agora va a sala de escritorios\ne fale com o tecnico Carlos.\nEle tem uma tarefa pra vc!"
};

/* ============================================================
 *  DIALOGO DO TRABALHADOR (intro)
 * ============================================================ */
#define WORKER_INTRO_STEPS 3
static const char *k_worker_intro[WORKER_INTRO_STEPS] = {
    "Ola! Sou o Carlos, tecnico de TI.\nPercebemos uma senha fraca numa\nestacao de trabalho deste andar.",
    "A senha encontrada foi: Admin123\nEste tipo de senha e facilmente\nbrutada em segundos por atacantes.",
    "Sua tarefa: identificar qual das\nsenhas abaixo e considerada\nFRACA e deve ser trocada."
};

/* ============================================================
 *  MINI-TASK: senha fraca
 * ============================================================ */
#define TASK_OPTIONS 3
static const char *k_task_opts[TASK_OPTIONS] = {
    "A) Admin123   <- senha fraca!",
    "B) xK#9mP!2@q <- senha forte",
    "C) P@ssw0rd99 <- fraca tambem!"
};
/* Opcoes corretas: 0 (A) e 2 (C) */

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
static void update_player_sprite(void);

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

static void show_secretaria_step(int step);
static void show_worker_intro_step(int step);
static void show_worker_task(void);
static void show_event_dialog(int slot);
static void close_dialog(void);

static bool near_secretaria(void);
static bool near_worker(void);

static void tmr_move_cb (lv_timer_t *t);
static void tmr_clock_cb(lv_timer_t *t);
static void tmr_event_cb(lv_timer_t *t);
static void tmr_drain_cb(lv_timer_t *t);
static void tmr_anim_cb (lv_timer_t *t);

static lv_color_t  ev_color(EventType t);
static const char *ev_label(EventType t);
static const char *ev_icon (EventType t);

/* ============================================================
 *  API PUBLICA
 * ============================================================ */

void cybersec_start(void)
{
    srand((unsigned int)time(NULL));

    g_state             = GS_TITLE;
    g_room              = ROOM_1;
    g_game_running      = false;
    g_integrity         = 100;
    g_game_hour         = 8;
    g_game_min          = 0;
    g_clock_ticks       = 0;
    g_nfc_ready         = true;
    /* Nasce na porta de entrada no centro superior da Sala 1 */
    g_plr.x             = SCR_W / 2 - PLR_FRAME_W;
    g_plr.y             = GAME_Y + 20;
    g_score             = 0;
    g_sec_step          = 0;
    g_worker_step       = 0;
    g_worker_task_done  = false;
    g_secretaria_done   = false;
    g_plr_frame         = 0;
    g_plr_dir           = PLR_DIR_DOWN;
    g_plr_moving        = false;
    g_task_selected     = 0;
    g_dlg_overlay       = NULL;
    g_hud_cont          = NULL;
    g_room_cont         = NULL;
    g_player_img        = NULL;
    g_player_cont       = NULL;
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

    /* Reiniciar — sempre */
    if ((uint32_t)sdlk == SDLK_r || (uint32_t)sdlk == SDLK_R) {
        do_restart();
        return;
    }

    /* --- TELA INICIAL --- */
    if (g_state == GS_TITLE) {
        if ((uint32_t)sdlk == SDLK_a     || (uint32_t)sdlk == SDLK_A ||
            (uint32_t)sdlk == SDLK_SPACE || (uint32_t)sdlk == SDLK_RETURN) {
            lv_obj_clean(lv_scr_act());
            lv_obj_set_style_bg_color(lv_scr_act(), lv_color_hex(0x000000), 0);
            create_hud();
            build_room1();
            create_player();
            update_hud();
            set_status("Fale com a secretaria", lv_color_hex(0x00FFAA));
            if (!g_tmr_move) g_tmr_move = lv_timer_create(tmr_move_cb, MOVE_MS, NULL);
            if (!g_tmr_anim) g_tmr_anim = lv_timer_create(tmr_anim_cb, ANIM_MS, NULL);
            g_state = GS_SECRETARIA;
        }
        return;
    }

    /* --- DIALOGO DA SECRETARIA (aberto) --- */
    if (g_state == GS_SECRETARIA && g_dlg_overlay != NULL) {
        if ((uint32_t)sdlk == SDLK_a     || (uint32_t)sdlk == SDLK_A ||
            (uint32_t)sdlk == SDLK_SPACE || (uint32_t)sdlk == SDLK_RETURN) {
            close_dialog();
            g_sec_step++;
            if (g_sec_step >= SEC_STEPS) {
                g_secretaria_done = true;
                set_status("Va a sala 2 e fale com Carlos!", lv_color_hex(0x00FFAA));
            } else {
                show_secretaria_step(g_sec_step);
            }
        }
        return;
    }

    /* --- SECRETARIA: livre (sem dialogo) --- */
    if (g_state == GS_SECRETARIA && g_dlg_overlay == NULL) {
        if ((uint32_t)sdlk == SDLK_a     || (uint32_t)sdlk == SDLK_A ||
            (uint32_t)sdlk == SDLK_SPACE || (uint32_t)sdlk == SDLK_RETURN) {
            if (!g_secretaria_done && near_secretaria()) {
                g_sec_step = 0;
                show_secretaria_step(0);
            } else if (g_secretaria_done) {
                set_status("Va a sala 2.", lv_color_hex(0x00FFAA));
            } else {
                set_status("Fale com a secretaria!", lv_color_hex(0xFFAA00));
            }
        }
        return;
    }

    /* --- INTRO TRABALHADOR (dialogo aberto) --- */
    if (g_state == GS_WORKER_INTRO && g_dlg_overlay != NULL) {
        if ((uint32_t)sdlk == SDLK_a     || (uint32_t)sdlk == SDLK_A ||
            (uint32_t)sdlk == SDLK_SPACE || (uint32_t)sdlk == SDLK_RETURN) {
            close_dialog();
            g_worker_step++;
            if (g_worker_step >= WORKER_INTRO_STEPS) {
                g_task_selected = 0;
                show_worker_task();
                g_state = GS_WORKER_TASK;
            } else {
                show_worker_intro_step(g_worker_step);
            }
        }
        return;
    }

    /* --- INTRO TRABALHADOR: livre --- */
    if (g_state == GS_WORKER_INTRO && g_dlg_overlay == NULL) {
        if ((uint32_t)sdlk == SDLK_a     || (uint32_t)sdlk == SDLK_A ||
            (uint32_t)sdlk == SDLK_SPACE || (uint32_t)sdlk == SDLK_RETURN) {
            if (near_worker()) {
                g_worker_step = 0;
                show_worker_intro_step(0);
            } else {
                set_status("Fale com Carlos!", lv_color_hex(0xFFAA00));
            }
        }
        return;
    }

    /* --- MINI-TASK SENHA --- */
    if (g_state == GS_WORKER_TASK && g_dlg_overlay != NULL) {
        if ((uint32_t)sdlk == SDLK_UP) {
            if (g_task_selected > 0) g_task_selected--;
            show_worker_task();
            return;
        }
        if ((uint32_t)sdlk == SDLK_DOWN) {
            if (g_task_selected < TASK_OPTIONS - 1) g_task_selected++;
            show_worker_task();
            return;
        }
        if ((uint32_t)sdlk == SDLK_a     || (uint32_t)sdlk == SDLK_A ||
            (uint32_t)sdlk == SDLK_SPACE || (uint32_t)sdlk == SDLK_RETURN) {
            close_dialog();
            /* Opcoes 0 (A) e 2 (C) sao fracas — corretas */
            if (g_task_selected == 0 || g_task_selected == 2) {
                g_worker_task_done = true;
                g_score += 20;
                set_status("Correto! Senha fraca identificada. +20pts", lv_color_hex(0x00FF88));
                /* Inicia o jogo principal */
                g_state        = GS_PLAYING;
                g_game_running = true;
                start_timers();
            } else {
                set_status("Errado! Aquela e uma senha forte. Tente novamente.", lv_color_hex(0xFF4444));
                g_state       = GS_WORKER_INTRO;
                g_worker_step = 0;
            }
        }
        return;
    }

    /* --- DIALOGO DE EVENTO (jogo principal) --- */
    if (g_dlg_overlay != NULL && g_state == GS_PLAYING) {
        bool is_nfc = ((uint32_t)sdlk == SDLK_n || (uint32_t)sdlk == SDLK_N);
        bool is_act = ((uint32_t)sdlk == SDLK_a || (uint32_t)sdlk == SDLK_A ||
                       (uint32_t)sdlk == SDLK_SPACE || (uint32_t)sdlk == SDLK_RETURN);
        if (!is_act && !is_nfc) return;

        int slot = nearest_event();
        if (slot >= 0) {
            EventType t = g_events[slot].type;
            if (t == EV_CRITICAL && !is_nfc) {
                close_dialog();
                set_status("Ransomware! Use [N] para NFC scan.", lv_color_hex(0xFF3333));
            } else if (t == EV_CRITICAL && is_nfc) {
                close_dialog();
                resolve_event(slot, true);
            } else {
                close_dialog();
                resolve_event(slot, false);
            }
        } else {
            close_dialog();
        }
        return;
    }

    if (g_state != GS_PLAYING) return;

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
    /* Fundo — 963x640 -> 480x320: scale=128 (50%) com pivot no canto sup-esq */
    lv_obj_t *bg_img = lv_image_create(lv_scr_act());
    lv_image_set_src(bg_img, IMG_TITLE);
    lv_image_set_pivot(bg_img, 0, 0);
    lv_image_set_scale(bg_img, 128);
    lv_obj_set_pos(bg_img, 0, 0);

    /* Overlay escuro leve */
    lv_obj_t *ov = lv_obj_create(lv_scr_act());
    lv_obj_set_size(ov, SCR_W, SCR_H);
    lv_obj_set_pos(ov, 0, 0);
    lv_obj_set_style_bg_color(ov, lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_opa(ov, LV_OPA_20, 0);
    lv_obj_set_style_border_width(ov, 0, 0);
    lv_obj_clear_flag(ov, LV_OBJ_FLAG_SCROLLABLE);

    /* Instrucao */
    lv_obj_t *lbl = lv_label_create(lv_scr_act());
    lv_label_set_text(lbl, "Pressione [A] para iniciar");
    lv_obj_set_style_text_color(lbl, lv_color_hex(0x00FFAA), 0);
    lv_obj_set_style_text_font(lbl, &lv_font_montserrat_14, 0);
    lv_obj_align(lbl, LV_ALIGN_BOTTOM_MID, 0, -18);
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
    lv_obj_set_style_border_width(g_hud_cont, 1, 0);
    lv_obj_set_style_border_side(g_hud_cont, LV_BORDER_SIDE_BOTTOM, 0);
    lv_obj_set_style_border_color(g_hud_cont, lv_color_hex(0x00CFAA), 0);
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
    lv_obj_set_width(g_lbl_status, 145);
    lv_label_set_long_mode(g_lbl_status, LV_LABEL_LONG_CLIP);
    lv_obj_set_style_text_align(g_lbl_status, LV_TEXT_ALIGN_RIGHT, 0);
    lv_obj_align(g_lbl_status, LV_ALIGN_RIGHT_MID, -4, 0);
}

/* ============================================================
 *  SALA 1 — Recepcao
 * ============================================================ */
static void build_room1(void)
{
    for (int i = 0; i < MAX_EVENTS; i++) {
        g_events[i].bubble     = NULL;
        g_events[i].bubble_lbl = NULL;
    }
    g_player_img  = NULL;
    g_player_cont = NULL;
    if (g_room_cont) { lv_obj_del(g_room_cont); g_room_cont = NULL; }

    g_room_cont = lv_obj_create(lv_scr_act());
    lv_obj_set_size(g_room_cont, SCR_W, GAME_H);
    lv_obj_set_pos(g_room_cont, 0, GAME_Y);
    lv_obj_set_style_bg_color(g_room_cont, lv_color_hex(0x202020), 0);
    lv_obj_set_style_border_width(g_room_cont, 0, 0);
    lv_obj_set_style_pad_all(g_room_cont, 0, 0);
    lv_obj_clear_flag(g_room_cont, LV_OBJ_FLAG_SCROLLABLE);

    /* Fundo — 959x635 -> 480x290: scale_x=128, scale_y=117 com pivot (0,0) */
    lv_obj_t *bg = lv_image_create(g_room_cont);
    lv_image_set_src(bg, IMG_SALA1);
    lv_image_set_pivot(bg, 0, 0);
    lv_image_set_scale_x(bg, 128);
    lv_image_set_scale_y(bg, 117);
    lv_obj_set_pos(bg, 0, 0);

    /* Secretaria — container 32x64 faz clipping, imagem 2x dentro */
    lv_obj_t *sec_clip = lv_obj_create(g_room_cont);
    lv_obj_set_size(sec_clip, NPC_FRAME_W * 2, NPC_FRAME_H * 2);
    lv_obj_set_pos(sec_clip, SEC_X - NPC_FRAME_W, SEC_Y - NPC_FRAME_H * 2);
    lv_obj_set_style_pad_all(sec_clip, 0, 0);
    lv_obj_set_style_border_width(sec_clip, 0, 0);
    lv_obj_set_style_radius(sec_clip, 0, 0);
    lv_obj_set_style_bg_opa(sec_clip, LV_OPA_TRANSP, 0);
    lv_obj_clear_flag(sec_clip, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_t *sec_spr = lv_image_create(sec_clip);
    lv_image_set_src(sec_spr, IMG_SECRETARIO);
    lv_image_set_pivot(sec_spr, 0, 0);
    lv_image_set_scale(sec_spr, 512);
    lv_obj_set_pos(sec_spr, 0, 0); /* frame 0 col=0, row=0 */

    /* Indicador [A] acima da secretaria */
    if (!g_secretaria_done) {
        lv_obj_t *hint = lv_label_create(g_room_cont);
        lv_label_set_text(hint, "[A]");
        lv_obj_set_style_text_color(hint, lv_color_hex(0x00FFAA), 0);
        lv_obj_set_style_text_font(hint, &lv_font_montserrat_10, 0);
        lv_obj_set_pos(hint, SEC_X - 8, SEC_Y - NPC_FRAME_H * 2 - 14);
    }

    /* Seta sala 2 */
    lv_obj_t *arrow = lv_label_create(g_room_cont);
    lv_label_set_text(arrow, "> Sala 2");
    lv_obj_set_style_text_color(arrow, lv_color_hex(0x00FFAA), 0);
    lv_obj_set_style_text_font(arrow, &lv_font_montserrat_10, 0);
    lv_obj_set_pos(arrow, SCR_W - 60, GAME_H / 2 - 6);
}

/* ============================================================
 *  SALA 2 — Escritorios
 * ============================================================ */
static void build_room2(void)
{
    for (int i = 0; i < MAX_EVENTS; i++) {
        g_events[i].bubble     = NULL;
        g_events[i].bubble_lbl = NULL;
    }
    g_player_img  = NULL;
    g_player_cont = NULL;
    if (g_room_cont) { lv_obj_del(g_room_cont); g_room_cont = NULL; }

    g_room_cont = lv_obj_create(lv_scr_act());
    lv_obj_set_size(g_room_cont, SCR_W, GAME_H);
    lv_obj_set_pos(g_room_cont, 0, GAME_Y);
    lv_obj_set_style_bg_color(g_room_cont, lv_color_hex(0x202020), 0);
    lv_obj_set_style_border_width(g_room_cont, 0, 0);
    lv_obj_set_style_pad_all(g_room_cont, 0, 0);
    lv_obj_clear_flag(g_room_cont, LV_OBJ_FLAG_SCROLLABLE);

    /* Fundo — 960x635 -> 480x290: scale_x=128, scale_y=117 com pivot (0,0) */
    lv_obj_t *bg = lv_image_create(g_room_cont);
    lv_image_set_src(bg, IMG_SALA2);
    lv_image_set_pivot(bg, 0, 0);
    lv_image_set_scale_x(bg, 128);
    lv_image_set_scale_y(bg, 117);
    lv_obj_set_pos(bg, 0, 0);

    /* NPC generico — container clipping 32x64 */
    lv_obj_t *npc2_clip = lv_obj_create(g_room_cont);
    lv_obj_set_size(npc2_clip, NPC_FRAME_W * 2, NPC_FRAME_H * 2);
    lv_obj_set_pos(npc2_clip, 180, 120);
    lv_obj_set_style_pad_all(npc2_clip, 0, 0);
    lv_obj_set_style_border_width(npc2_clip, 0, 0);
    lv_obj_set_style_radius(npc2_clip, 0, 0);
    lv_obj_set_style_bg_opa(npc2_clip, LV_OPA_TRANSP, 0);
    lv_obj_clear_flag(npc2_clip, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_t *npc2 = lv_image_create(npc2_clip);
    lv_image_set_src(npc2, IMG_NPC_SALA2);
    lv_image_set_pivot(npc2, 0, 0);
    lv_image_set_scale(npc2, 512);
    lv_obj_set_pos(npc2, 0, 0);

    /* Trabalhador Carlos — container clipping 32x64, inferior direito */
    lv_obj_t *wrk_clip = lv_obj_create(g_room_cont);
    lv_obj_set_size(wrk_clip, NPC_FRAME_W * 2, NPC_FRAME_H * 2);
    lv_obj_set_pos(wrk_clip, WORKER_X, WORKER_Y);
    lv_obj_set_style_pad_all(wrk_clip, 0, 0);
    lv_obj_set_style_border_width(wrk_clip, 0, 0);
    lv_obj_set_style_radius(wrk_clip, 0, 0);
    lv_obj_set_style_bg_opa(wrk_clip, LV_OPA_TRANSP, 0);
    lv_obj_clear_flag(wrk_clip, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_t *worker = lv_image_create(wrk_clip);
    lv_image_set_src(worker, IMG_TRABALHADOR);
    lv_image_set_pivot(worker, 0, 0);
    lv_image_set_scale(worker, 512);
    lv_obj_set_pos(worker, 0, 0);

    /* Indicador [A] acima do trabalhador */
    if (!g_worker_task_done) {
        lv_obj_t *hint = lv_label_create(g_room_cont);
        lv_label_set_text(hint, "[A]");
        lv_obj_set_style_text_color(hint, lv_color_hex(0xFFAA00), 0);
        lv_obj_set_style_text_font(hint, &lv_font_montserrat_10, 0);
        lv_obj_set_pos(hint, WORKER_X + NPC_FRAME_W / 2, WORKER_Y - 14);
    }

    /* Seta de volta */
    lv_obj_t *back = lv_label_create(g_room_cont);
    lv_label_set_text(back, "< Sala 1");
    lv_obj_set_style_text_color(back, lv_color_hex(0x00FFAA), 0);
    lv_obj_set_style_text_font(back, &lv_font_montserrat_10, 0);
    lv_obj_set_pos(back, 2, GAME_H / 2 - 6);
}

/* ============================================================
 *  PERSONAGEM
 * ============================================================ */
static void create_player(void)
{
    /* Container de clipping define posicao e tamanho visivel (24x32 display = 2x de 12x16) */
    g_player_cont = lv_obj_create(g_room_cont);
    lv_obj_set_size(g_player_cont, PLR_FRAME_W * 2, PLR_FRAME_H * 2);
    lv_obj_set_style_pad_all(g_player_cont, 0, 0);
    lv_obj_set_style_border_width(g_player_cont, 0, 0);
    lv_obj_set_style_radius(g_player_cont, 0, 0);
    lv_obj_set_style_bg_opa(g_player_cont, LV_OPA_TRANSP, 0);
    lv_obj_clear_flag(g_player_cont, LV_OBJ_FLAG_SCROLLABLE);

    /* Imagem do sprite sheet dentro do container */
    g_player_img = lv_image_create(g_player_cont);
    lv_image_set_src(g_player_img, IMG_PLAYER);
    lv_image_set_pivot(g_player_img, 0, 0);
    lv_image_set_scale(g_player_img, 512); /* 2x */
    g_plr_frame = 0;
    g_plr_dir   = PLR_DIR_DOWN;
    update_player_sprite();
    update_player_pos();
}

static void update_player_sprite(void)
{
    if (!g_player_img) return;
    /* Mover a imagem dentro do container para selecionar o frame correto.
     * Cada frame tem PLR_FRAME_W*2 px (display 2x). Offset negativo = move imagem para a esquerda. */
    int disp_fw = PLR_FRAME_W * 2;   /* largura de 1 frame em display pixels (24) */
    int disp_fh = PLR_FRAME_H * 2;   /* altura  de 1 frame em display pixels (32) */
    lv_obj_set_pos(g_player_img,
                   -(g_plr_frame * disp_fw),
                   -(g_plr_dir   * disp_fh));
}

/* ============================================================
 *  TIMERS
 * ============================================================ */
static void start_timers(void)
{
    if (!g_tmr_clock) g_tmr_clock = lv_timer_create(tmr_clock_cb, CLOCK_TICK_MS, NULL);
    if (!g_tmr_event) g_tmr_event = lv_timer_create(tmr_event_cb, EVENT_MIN_MS,  NULL);
    if (!g_tmr_drain) g_tmr_drain = lv_timer_create(tmr_drain_cb, DRAIN_TICK_MS, NULL);
}

static void stop_timers(void)
{
    if (g_tmr_move)  { lv_timer_del(g_tmr_move);  g_tmr_move  = NULL; }
    if (g_tmr_clock) { lv_timer_del(g_tmr_clock); g_tmr_clock = NULL; }
    if (g_tmr_event) { lv_timer_del(g_tmr_event); g_tmr_event = NULL; }
    if (g_tmr_drain) { lv_timer_del(g_tmr_drain); g_tmr_drain = NULL; }
    if (g_tmr_anim)  { lv_timer_del(g_tmr_anim);  g_tmr_anim  = NULL; }
}

static void tmr_anim_cb(lv_timer_t *t)
{
    (void)t;
    if (!g_player_img) return;
    if (g_plr_moving) {
        g_plr_frame = (g_plr_frame + 1) % PLR_FRAMES;
    } else {
        g_plr_frame = 0;
    }
    update_player_sprite();
}

static void tmr_move_cb(lv_timer_t *t)
{
    (void)t;
    if (g_dlg_overlay) return;

    bool moving = g_keys.up || g_keys.down || g_keys.left || g_keys.right;
    g_plr_moving = moving;

    if (g_keys.up)    { g_plr.y -= PLR_SPEED; g_plr_dir = PLR_DIR_UP;    }
    if (g_keys.down)  { g_plr.y += PLR_SPEED; g_plr_dir = PLR_DIR_DOWN;  }
    if (g_keys.left)  { g_plr.x -= PLR_SPEED; g_plr_dir = PLR_DIR_LEFT;  }
    if (g_keys.right) { g_plr.x += PLR_SPEED; g_plr_dir = PLR_DIR_RIGHT; }

    /* Transicoes de sala — verificar APOS mover, antes do clamp.
     * Faixa Y restrita a abertura real da parede nos backgrounds.
     * Ajuste DOOR_TOP / DOOR_BOT para bater com o recorte visual. */
#define DOOR_TOP  55
#define DOOR_BOT  135
    if (g_secretaria_done || g_state == GS_PLAYING) {
        if (g_room == ROOM_1 &&
            g_plr.x >= SCR_W - PLR_W * 2 - 10 &&
            g_plr.y > GAME_Y + DOOR_TOP && g_plr.y < GAME_Y + DOOR_BOT) {
            switch_room(ROOM_2);
            return;
        }
        if (g_room == ROOM_2 &&
            g_plr.x <= 0 &&
            g_plr.y > GAME_Y + DOOR_TOP && g_plr.y < GAME_Y + DOOR_BOT) {
            switch_room(ROOM_1);
            return;
        }
    }
#undef DOOR_TOP
#undef DOOR_BOT

    int min_x = 0;
    if (g_plr.x < min_x)           g_plr.x = min_x;
    if (g_plr.x > SCR_W - PLR_W * 2)  g_plr.x = SCR_W - PLR_W * 2;
    if (g_plr.y < GAME_Y + 2)      g_plr.y = GAME_Y + 2;
    if (g_plr.y > SCR_H - PLR_H * 2)  g_plr.y = SCR_H - PLR_H * 2;

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
static bool near_secretaria(void)
{
    int ex = SEC_X;
    int ey = (SEC_Y - NPC_FRAME_H) + GAME_Y;  /* centro visual do NPC em coords de tela */
    int dx = g_plr.x + PLR_W - ex;
    int dy = g_plr.y + PLR_H - ey;
    return (dx * dx + dy * dy) < (NPC_INTERACT_R * NPC_INTERACT_R);
}

static bool near_worker(void)
{
    int ex = WORKER_X + NPC_FRAME_W;
    int ey = WORKER_Y + NPC_FRAME_H + GAME_Y;
    int dx = g_plr.x + PLR_W - ex;
    int dy = g_plr.y + PLR_H - ey;
    return (dx * dx + dy * dy) < (NPC_INTERACT_R * NPC_INTERACT_R);
}

static void switch_room(RoomID room)
{
    g_room = room;
    memset(&g_keys, 0, sizeof(g_keys));
    g_plr_moving = false;

    if (room == ROOM_1) {
        g_plr.x = SCR_W - PLR_W * 2 - 16;
        g_plr.y = GAME_Y + 90;   /* centro da abertura da porta */
        build_room1();
        lv_label_set_text(g_lbl_room_name, "Recepcao");
        set_status("Sala Principal.", lv_color_hex(0x44CC88));
    } else {
        g_plr.x = 18;
        g_plr.y = GAME_Y + 90;   /* centro da abertura da porta */
        build_room2();
        lv_label_set_text(g_lbl_room_name, "Escritorios");
        if (!g_worker_task_done && g_state == GS_SECRETARIA) {
            g_state = GS_WORKER_INTRO;
            set_status("Fale com Carlos.", lv_color_hex(0xFFAA00));
        } else {
            set_status("Escritorios - Andar 2.", lv_color_hex(0x44CC88));
        }
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
             (ev->room == ROOM_1) ? "Recepcao" : "Escritorios");
    set_status(buf, ev_color(ev->type));

    if (ev->type == EV_CRITICAL)
        lv_obj_set_style_bg_color(g_hud_cont, lv_color_hex(0x280808), 0);
}

static void resolve_event(int slot, bool nfc)
{
    if (!g_events[slot].active) return;
    if (g_events[slot].bubble) {
        lv_obj_del(g_events[slot].bubble);
        g_events[slot].bubble     = NULL;
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
        int dx = g_plr.x + PLR_W - ex;
        int dy = g_plr.y + PLR_H - ey;
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

    lv_obj_t *sep2 = lv_obj_create(box);
    lv_obj_set_size(sep2, 290, 1);
    lv_obj_set_style_bg_color(sep2, lv_color_hex(0x1E3A50), 0);
    lv_obj_set_style_border_width(sep2, 0, 0);
    lv_obj_align(sep2, LV_ALIGN_TOP_MID, 0, 22);
    lv_obj_clear_flag(sep2, LV_OBJ_FLAG_SCROLLABLE);

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

/* Cria caixa de dialogo com portrait NPC (sprite sheet lateral) */
static void create_npc_dialog(const char *sprite_src, const char *text,
                               int step, int total)
{
    if (g_dlg_overlay) { lv_obj_del(g_dlg_overlay); g_dlg_overlay = NULL; }

    g_dlg_overlay = lv_obj_create(lv_scr_act());
    lv_obj_set_size(g_dlg_overlay, SCR_W, SCR_H);
    lv_obj_set_pos(g_dlg_overlay, 0, 0);
    lv_obj_set_style_bg_color(g_dlg_overlay, lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_opa(g_dlg_overlay, LV_OPA_40, 0);
    lv_obj_set_style_border_width(g_dlg_overlay, 0, 0);
    lv_obj_clear_flag(g_dlg_overlay, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *box = lv_obj_create(g_dlg_overlay);
    lv_obj_set_size(box, SCR_W - 10, 132);
    lv_obj_set_pos(box, 5, SCR_H - 140);
    lv_obj_set_style_bg_color(box, lv_color_hex(0x030608), 0);
    lv_obj_set_style_bg_opa(box, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(box, lv_color_hex(0x1E3050), 0);
    lv_obj_set_style_border_width(box, 2, 0);
    lv_obj_set_style_radius(box, 4, 0);
    lv_obj_set_style_pad_all(box, 0, 0);
    lv_obj_clear_flag(box, LV_OBJ_FLAG_SCROLLABLE);

    /* Frame de portrait */
    lv_obj_t *pframe = lv_obj_create(box);
    lv_obj_set_size(pframe, 72, 116);
    lv_obj_set_pos(pframe, 6, 8);
    lv_obj_set_style_bg_color(pframe, lv_color_hex(0x080C18), 0);
    lv_obj_set_style_border_color(pframe, lv_color_hex(0x1E3050), 0);
    lv_obj_set_style_border_width(pframe, 1, 0);
    lv_obj_set_style_radius(pframe, 2, 0);
    lv_obj_set_style_pad_all(pframe, 0, 0);
    lv_obj_clear_flag(pframe, LV_OBJ_FLAG_SCROLLABLE);

    /* Sprite NPC no portrait — container clipping 72x116, imagem 4.5x dentro */
    lv_obj_t *npc_port_clip = lv_obj_create(pframe);
    lv_obj_set_size(npc_port_clip, 72, 116);
    lv_obj_set_pos(npc_port_clip, 0, 0);
    lv_obj_set_style_pad_all(npc_port_clip, 0, 0);
    lv_obj_set_style_border_width(npc_port_clip, 0, 0);
    lv_obj_set_style_radius(npc_port_clip, 0, 0);
    lv_obj_set_style_bg_opa(npc_port_clip, LV_OPA_TRANSP, 0);
    lv_obj_clear_flag(npc_port_clip, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_t *npc_portrait = lv_image_create(npc_port_clip);
    lv_image_set_src(npc_portrait, sprite_src);
    lv_image_set_pivot(npc_portrait, 0, 0);
    lv_image_set_scale(npc_portrait, 1152); /* 4.5x: 16px->72, 32px->144 */
    lv_obj_set_pos(npc_portrait, 0, 0);

    /* Texto do dialogo */
    lv_obj_t *txt = lv_label_create(box);
    lv_label_set_text(txt, text);
    lv_obj_set_style_text_color(txt, lv_color_hex(0xDDEEFF), 0);
    lv_obj_set_style_text_font(txt, &lv_font_montserrat_14, 0);
    lv_obj_set_size(txt, SCR_W - 110, 98);
    lv_obj_set_pos(txt, 84, 8);
    lv_label_set_long_mode(txt, LV_LABEL_LONG_WRAP);

    /* Indicador de continuacao */
    lv_obj_t *pg = lv_label_create(box);
    char pbuf[32];
    snprintf(pbuf, sizeof(pbuf), "[A] continuar  %d/%d", step + 1, total);
    lv_label_set_text(pg, pbuf);
    lv_obj_set_style_text_color(pg, lv_color_hex(0x5588AA), 0);
    lv_obj_set_style_text_font(pg, &lv_font_montserrat_10, 0);
    lv_obj_align(pg, LV_ALIGN_BOTTOM_RIGHT, -6, -4);
}

static void show_secretaria_step(int step)
{
    create_npc_dialog(IMG_SECRETARIO, k_sec_dialog[step], step, SEC_STEPS);
}

static void show_worker_intro_step(int step)
{
    create_npc_dialog(IMG_TRABALHADOR, k_worker_intro[step], step, WORKER_INTRO_STEPS);
}

static void show_worker_task(void)
{
    if (g_dlg_overlay) { lv_obj_del(g_dlg_overlay); g_dlg_overlay = NULL; }

    g_dlg_overlay = lv_obj_create(lv_scr_act());
    lv_obj_set_size(g_dlg_overlay, SCR_W, SCR_H);
    lv_obj_set_pos(g_dlg_overlay, 0, 0);
    lv_obj_set_style_bg_color(g_dlg_overlay, lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_opa(g_dlg_overlay, LV_OPA_60, 0);
    lv_obj_set_style_border_width(g_dlg_overlay, 0, 0);
    lv_obj_clear_flag(g_dlg_overlay, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *box = lv_obj_create(g_dlg_overlay);
    lv_obj_set_size(box, 340, 220);
    lv_obj_center(box);
    lv_obj_set_style_bg_color(box, lv_color_hex(0x03060A), 0);
    lv_obj_set_style_bg_opa(box, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(box, lv_color_hex(0xFFAA00), 0);
    lv_obj_set_style_border_width(box, 2, 0);
    lv_obj_set_style_radius(box, 4, 0);
    lv_obj_set_style_pad_all(box, 12, 0);
    lv_obj_clear_flag(box, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *ttl = lv_label_create(box);
    lv_label_set_text(ttl, "! TAREFA: Identificar Senha Fraca");
    lv_obj_set_style_text_color(ttl, lv_color_hex(0xFFAA00), 0);
    lv_obj_set_style_text_font(ttl, &lv_font_montserrat_14, 0);
    lv_obj_align(ttl, LV_ALIGN_TOP_MID, 0, 0);

    lv_obj_t *sep = lv_obj_create(box);
    lv_obj_set_size(sep, 310, 1);
    lv_obj_set_style_bg_color(sep, lv_color_hex(0xFFAA00), 0);
    lv_obj_set_style_border_width(sep, 0, 0);
    lv_obj_align(sep, LV_ALIGN_TOP_MID, 0, 20);
    lv_obj_clear_flag(sep, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *q = lv_label_create(box);
    lv_label_set_text(q, "Qual das seguintes senhas\ne considerada FRACA e deve ser trocada?");
    lv_obj_set_style_text_color(q, lv_color_hex(0xCCDDEE), 0);
    lv_obj_set_style_text_font(q, &lv_font_montserrat_10, 0);
    lv_obj_set_width(q, 310);
    lv_obj_align(q, LV_ALIGN_TOP_MID, 0, 28);

    for (int i = 0; i < TASK_OPTIONS; i++) {
        lv_obj_t *opt = lv_label_create(box);
        lv_label_set_text(opt, k_task_opts[i]);
        lv_obj_set_style_text_font(opt, &lv_font_montserrat_10, 0);
        lv_color_t c = (i == g_task_selected) ?
                        lv_color_hex(0x00FFAA) : lv_color_hex(0x8899AA);
        lv_obj_set_style_text_color(opt, c, 0);
        lv_obj_set_width(opt, 310);
        lv_obj_set_pos(opt, 12, 75 + i * 24);
    }

    lv_obj_t *hint = lv_label_create(box);
    lv_label_set_text(hint, "[↑↓] Selecionar   [A] Confirmar");
    lv_obj_set_style_text_color(hint, lv_color_hex(0x5588AA), 0);
    lv_obj_set_style_text_font(hint, &lv_font_montserrat_10, 0);
    lv_obj_align(hint, LV_ALIGN_BOTTOM_MID, 0, -4);
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

    char ttl[80];
    snprintf(ttl, sizeof(ttl), "%s  %s", ev_icon(ev->type),
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

    lv_obj_t *sep = lv_obj_create(box);
    lv_obj_set_size(sep, 280, 1);
    lv_obj_set_style_bg_color(sep, ev_color(ev->type), 0);
    lv_obj_set_style_border_width(sep, 0, 0);
    lv_obj_align(sep, LV_ALIGN_TOP_MID, 0, 20);
    lv_obj_clear_flag(sep, LV_OBJ_FLAG_SCROLLABLE);

    char det[200];
    snprintf(det, sizeof(det),
             "* Sala:        %s\n"
             "* Usuario:     %s\n"
             "* Detalhe:     %s\n"
             "* Integridade: %d%%\n"
             "* Nivel risco: %s",
             (ev->room == ROOM_1) ? "Recepcao" : "Escritorios",
             ev->victim, ev->detail, ev->hp,
             ev->type == EV_CRITICAL ? "CRITICO" :
             ev->type == EV_ANOMALY  ? "Alto" : "Baixo");

    lv_obj_t *d = lv_label_create(box);
    lv_label_set_text(d, det);
    lv_obj_set_style_text_color(d, lv_color_hex(0xCCDDEE), 0);
    lv_obj_set_style_text_font(d, &lv_font_montserrat_10, 0);
    lv_obj_set_width(d, 280);
    lv_label_set_long_mode(d, LV_LABEL_LONG_WRAP);
    lv_obj_align(d, LV_ALIGN_TOP_MID, 0, 28);

    lv_obj_t *hbar = lv_bar_create(box);
    lv_obj_set_size(hbar, 280, 8);
    lv_obj_align(hbar, LV_ALIGN_BOTTOM_MID, 0, -20);
    lv_bar_set_range(hbar, 0, 100);
    lv_bar_set_value(hbar, ev->hp, LV_ANIM_OFF);
    lv_obj_set_style_bg_color(hbar, lv_color_hex(0x0E1E2E), 0);
    lv_obj_set_style_bg_color(hbar, ev_color(ev->type), LV_PART_INDICATOR);
    lv_obj_set_style_radius(hbar, 2, 0);
    lv_obj_set_style_radius(hbar, 2, LV_PART_INDICATOR);

    const char *act =
        (ev->type == EV_CRITICAL) ? "[ N = NFC scan para resolver ]" :
                                    "[ A = Corrigir vulnerabilidade ]";
    lv_obj_t *ai = lv_label_create(box);
    lv_label_set_text(ai, act);
    lv_obj_set_style_text_color(ai, ev_color(ev->type), 0);
    lv_obj_set_style_text_font(ai, &lv_font_montserrat_10, 0);
    lv_obj_align(ai, LV_ALIGN_BOTTOM_MID, 0, -4);
}

static void close_dialog(void)
{
    if (g_dlg_overlay) { lv_obj_del(g_dlg_overlay); g_dlg_overlay = NULL; }
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
    if (!g_player_cont) return;
    int ry = g_plr.y - GAME_Y;
    lv_obj_set_pos(g_player_cont, g_plr.x, ry);
}

static void rebuild_event_bubbles(void)
{
    for (int i = 0; i < MAX_EVENTS; i++) {
        if (g_events[i].bubble) {
            lv_obj_del(g_events[i].bubble);
            g_events[i].bubble     = NULL;
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

        g_events[i].bubble     = bub;
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
