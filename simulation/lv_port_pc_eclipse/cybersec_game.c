/**
 * =============================================================================
 * CyberSec: Network Defender — LVGL PC Simulator
 * Implementação completa do motor de jogo
 * =============================================================================
 *
 * MECÂNICAS IMPLEMENTADAS:
 *  - Mapa top-down com 4 salas (Recepção, R.Humanos, Financeiro, Servidores)
 *  - Personagem controlável via teclado (setas)
 *  - Máquina de Estados Finitos (FSM) de eventos: TAREFA / ANOMALIA / RANSOMWARE
 *  - Drenagem de HP por evento ignorado (escala por gravidade)
 *  - Mecânica NFC: tecla [N] simula aproximar cartão de backup
 *  - HUD: relógio 08h–18h, barra de integridade da rede, status do sistema
 *  - Game Over por integridade zero ou vitória ao chegar às 18h
 *  - Dialog contextual para NFC, boas-vindas e resultado
 *
 * ESTRUTURA DO ARQUIVO:
 *  1. Includes e constantes
 *  2. Tipos e estado global
 *  3. Declarações forward
 *  4. API pública (cybersec_start / cybersec_sdl_key_event)
 *  5. Criação de UI (HUD, mapa, salas, personagem)
 *  6. Callbacks de timer (movimentação, relógio, spawn de eventos, drenagem)
 *  7. Lógica de jogo (spawn, resolve, colisão, game-over)
 *  8. Atualização de UI (salas, HUD, posição do jogador)
 *  9. Helpers (cores, labels, dialog)
 * =============================================================================
 */

#include "cybersec_game.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* Mapeamento SDL_Keycode — evita dependência de header SDL neste arquivo.
 * Se SDL.h já estiver incluído no projeto, remova estas definições. */
#ifndef SDLK_UP
#define SDLK_UP         0x40000052
#define SDLK_DOWN       0x40000051
#define SDLK_LEFT       0x40000050
#define SDLK_RIGHT      0x4000004F
#define SDLK_SPACE      0x00000020
#define SDLK_RETURN     0x0000000D
#define SDLK_n          0x0000006E
#define SDLK_N          0x0000004E
#define SDLK_r          0x00000072
#define SDLK_R          0x00000052
#define SDLK_ESCAPE     0x0000001B
#endif

/* ============================================================
 *  CONSTANTES
 * ============================================================*/
#define SCR_W           480
#define SCR_H           320
#define HUD_H           36         /* Altura do HUD no topo           */
#define MAP_Y           HUD_H
#define MAP_H           (SCR_H - HUD_H)

/* Jogador */
#define PLR_SIZE        10         /* px — quadrado do sprite         */
#define PLR_SPEED       3          /* px por tick de movimento        */
#define MOVE_TICK_MS    16         /* ~62 fps para movimento suave    */

/* Relógio do jogo: 3 min reais = 10 horas de expediente (08h–18h) */
#define GAME_REAL_MS    (3 * 60 * 1000)
#define CLOCK_TICK_MS   500        /* atualiza relógio a cada 500 ms  */
/* Proporção: total_ticks = GAME_REAL_MS / CLOCK_TICK_MS = 360 ticks
 * 10h = 600 min; min_por_tick = 600/360 ≈ 1.67 → usamos frações */
#define GAME_TOTAL_TICKS    (GAME_REAL_MS / CLOCK_TICK_MS)  /* 360 */
#define GAME_TOTAL_MINUTES  600    /* 10h * 60 min                    */

/* Eventos */
#define EVENT_MIN_MS    7000       /* intervalo mínimo entre spawns   */
#define EVENT_MAX_MS    14000      /* intervalo máximo                */
#define DRAIN_TICK_MS   2000       /* drenagem de HP a cada 2 s       */
#define DRAIN_ROUTINE   2          /* HP drenado por tick (verde)     */
#define DRAIN_ANOMALY   5          /* HP drenado por tick (amarelo)   */
#define DRAIN_CRITICAL  12         /* HP drenado por tick (vermelho)  */

/* Salas */
#define NUM_ROOMS       4

/* ============================================================
 *  TIPOS
 * ============================================================*/

/** Tipo de evento/ameaça ativo numa sala */
typedef enum {
    EV_NONE     = 0,   /**< Sala operando normalmente             */
    EV_ROUTINE  = 1,   /**< Tarefa de rotina (verde)              */
    EV_ANOMALY  = 2,   /**< Anomalia suspeita (amarelo)           */
    EV_CRITICAL = 3,   /**< RANSOMWARE — exige NFC (vermelho)     */
} EventType;

/** Estado de uma sala no mapa */
typedef struct {
    const char  *name;         /**< Nome exibido na UI             */
    lv_coord_t   x, y;        /**< Posição absoluta (tela inteira) */
    lv_coord_t   w, h;        /**< Dimensões                       */
    int          health;       /**< Integridade 0–100              */
    EventType    event;        /**< Evento ativo                   */
} Room;

/** Estado do jogador */
typedef struct {
    lv_coord_t x, y;          /**< Posição absoluta (tela inteira) */
} Player;

/** Estado de input */
typedef struct {
    bool up, down, left, right;
} KeyState;

/* ============================================================
 *  ESTADO GLOBAL
 * ============================================================*/

/* Layout das salas — coordenadas absolutas (incluindo MAP_Y) */
static Room rooms[NUM_ROOMS] = {
    /* name           x    y            w    h    hp   ev      */
    {"Recepcao",       4,  MAP_Y+4,   227, 127,  100, EV_NONE},
    {"R. Humanos",   249,  MAP_Y+4,   227, 127,  100, EV_NONE},
    {"Financeiro",     4,  MAP_Y+153, 227, 127,  100, EV_NONE},
    {"Servidores",   249,  MAP_Y+153, 227, 127,  100, EV_NONE},
};

static Player   plr         = { 239, MAP_Y + 142 };
static KeyState keys        = { false, false, false, false };

static int      net_integrity   = 100;
static int      game_hour       = 8;
static int      game_min        = 0;
static int      clock_ticks     = 0;    /* ticks acumulados do relógio */
static bool     game_running    = false;
static bool     nfc_ready       = true; /* simula cartão NFC disponível */

/* ============================================================
 *  OBJETOS LVGL
 * ============================================================*/

/* HUD */
static lv_obj_t *hud_cont;
static lv_obj_t *lbl_time;
static lv_obj_t *lbl_status;
static lv_obj_t *bar_integrity;
static lv_obj_t *lbl_integrity_val;

/* Mapa */
static lv_obj_t *map_cont;
static lv_obj_t *room_panel     [NUM_ROOMS];
static lv_obj_t *room_lbl_name  [NUM_ROOMS];
static lv_obj_t *room_lbl_ev    [NUM_ROOMS];
static lv_obj_t *room_lbl_hp    [NUM_ROOMS];
static lv_obj_t *room_hp_bar    [NUM_ROOMS];
static lv_obj_t *player_icon;

/* Dialog de overlay */
static lv_obj_t *dlg_overlay = NULL;

/* Timers */
static lv_timer_t *tmr_move  = NULL;
static lv_timer_t *tmr_clock = NULL;
static lv_timer_t *tmr_event = NULL;
static lv_timer_t *tmr_drain = NULL;

/* ============================================================
 *  FORWARD DECLARATIONS
 * ============================================================*/
static void create_hud          (void);
static void create_map          (void);
static void create_player       (void);
static void start_timers        (void);
static void stop_and_del_timers (void);

static void tmr_move_cb  (lv_timer_t *t);
static void tmr_clock_cb (lv_timer_t *t);
static void tmr_event_cb (lv_timer_t *t);
static void tmr_drain_cb (lv_timer_t *t);

static void spawn_event      (int room_idx, EventType ev);
static void resolve_room     (int room_idx, bool nfc);
static void recalc_integrity (void);
static int  get_player_room  (void);

static void update_room_ui   (int idx);
static void update_hud_ui    (void);
static void update_player_pos(void);
static void set_status_msg   (const char *msg, lv_color_t color);

static void show_dialog      (const char *title, const char *msg,
                               bool is_game_end);
static void dlg_close_cb     (lv_event_t *e);
static void do_game_over     (bool victory);
static void do_restart       (void);

static lv_color_t ev_color   (EventType ev);
static const char *ev_label  (EventType ev);

/* ============================================================
 *  API PÚBLICA
 * ============================================================*/

void cybersec_start(void)
{
    srand((unsigned int)time(NULL));

    /* Limpa tela */
    lv_obj_clean(lv_scr_act());
    lv_obj_set_style_bg_color(lv_scr_act(), lv_color_hex(0x080f1a), 0);

    /* Reinicia estado */
    for (int i = 0; i < NUM_ROOMS; i++) {
        rooms[i].health = 100;
        rooms[i].event  = EV_NONE;
    }
    plr.x         = 239;
    plr.y         = MAP_Y + 142;
    net_integrity = 100;
    game_hour     = 8;
    game_min      = 0;
    clock_ticks   = 0;
    nfc_ready     = true;
    dlg_overlay   = NULL;
    game_running  = false; /* será true após fechar o dialog inicial */

    /* Constrói UI */
    create_hud();
    create_map();
    create_player();
    start_timers();

    /* Dialog de boas-vindas — pausa o jogo até fechar */
    show_dialog(
        "CyberSec: Network Defender",
        "Bem-vindo, Analista de TI!\n\n"
        "[Setas]   Mover pelo escritorio\n"
        "[SPACE]   Resolver tarefa/anomalia\n"
        "[N]       Scan NFC (Ransomware)\n"
        "[R]       Reiniciar jogo\n\n"
        "Proteja a rede ate as 18:00!\n"
        "Boa sorte!",
        false
    );
}

void cybersec_sdl_key_event(int32_t sdlk, bool is_down)
{
    /* Teclas de movimento — estado contínuo */
    if (sdlk == SDLK_UP)    { keys.up    = is_down; return; }
    if (sdlk == SDLK_DOWN)  { keys.down  = is_down; return; }
    if (sdlk == SDLK_LEFT)  { keys.left  = is_down; return; }
    if (sdlk == SDLK_RIGHT) { keys.right = is_down; return; }

    /* As demais ações só no momento do press (is_down = true) */
    if (!is_down) return;

    /* Reiniciar */
    if (sdlk == SDLK_r || sdlk == SDLK_R) {
        do_restart();
        return;
    }

    /* Fechar dialog com qualquer tecla de ação */
    if (dlg_overlay) {
        if (sdlk == SDLK_SPACE  ||
            sdlk == SDLK_RETURN ||
            sdlk == SDLK_n      ||
            sdlk == SDLK_N) {
            dlg_close_cb(NULL);
        }
        return;
    }

    if (!game_running) return;

    int room = get_player_room();

    /* Interagir (verde / amarelo) */
    if (sdlk == SDLK_SPACE || sdlk == SDLK_RETURN) {
        if (room < 0) {
            set_status_msg("Entre em uma sala primeiro.", lv_color_hex(0x888888));
            return;
        }
        if (rooms[room].event == EV_NONE) {
            set_status_msg("Nenhum incidente nesta sala.", lv_color_hex(0x888888));
            return;
        }
        if (rooms[room].event == EV_CRITICAL) {
            set_status_msg("RANSOMWARE! Use [N] para scan NFC.", lv_color_hex(0xff4444));
            return;
        }
        resolve_room(room, false);
        return;
    }

    /* NFC scan (vermelho) */
    if (sdlk == SDLK_n || sdlk == SDLK_N) {
        if (room < 0) {
            set_status_msg("Entre em uma sala primeiro.", lv_color_hex(0x888888));
            return;
        }
        if (rooms[room].event != EV_CRITICAL) {
            set_status_msg("Nenhum Ransomware ativo aqui.", lv_color_hex(0x888888));
            return;
        }
        if (!nfc_ready) {
            show_dialog("NFC Indisponivel",
                        "Cartao de backup nao detectado!\n\n"
                        "Aproxime o cartao NFC ao console\n"
                        "e tente novamente.",
                        false);
            return;
        }
        /* Resolve com animação de dialog */
        show_dialog("NFC Autenticado!",
                    "Cartao de Backup verificado.\n\n"
                    "Iniciando recuperacao de desastre...\n"
                    "Sistema restaurado com sucesso!",
                    false);
        resolve_room(room, true);
        return;
    }
}

/* ============================================================
 *  CRIAÇÃO DE UI
 * ============================================================*/

static void create_hud(void)
{
    hud_cont = lv_obj_create(lv_scr_act());
    lv_obj_set_size(hud_cont, SCR_W, HUD_H);
    lv_obj_set_pos(hud_cont, 0, 0);
    lv_obj_set_style_bg_color(hud_cont, lv_color_hex(0x0b1624), 0);
    lv_obj_set_style_border_width(hud_cont, 0, 0);
    lv_obj_set_style_border_side(hud_cont,
        LV_BORDER_SIDE_BOTTOM, 0);
    lv_obj_set_style_border_color(hud_cont, lv_color_hex(0x00ffc8), 0);
    lv_obj_set_style_border_width(hud_cont, 1, 0);
    lv_obj_set_style_pad_all(hud_cont, 4, 0);
    lv_obj_clear_flag(hud_cont, LV_OBJ_FLAG_SCROLLABLE);

    /* ---- Relógio (esquerda) ---- */
    lbl_time = lv_label_create(hud_cont);
    lv_label_set_text(lbl_time, "08:00");
    lv_obj_set_style_text_color(lbl_time, lv_color_hex(0x00ffc8), 0);
    lv_obj_set_style_text_font(lbl_time, &lv_font_montserrat_14, 0);
    lv_obj_align(lbl_time, LV_ALIGN_LEFT_MID, 2, 0);

    /* ---- Barra de integridade (centro) ---- */
    lv_obj_t *lbl_int_title = lv_label_create(hud_cont);
    lv_label_set_text(lbl_int_title, "REDE");
    lv_obj_set_style_text_font(lbl_int_title, &lv_font_montserrat_10, 0);
    lv_obj_set_style_text_color(lbl_int_title, lv_color_hex(0x4488aa), 0);
    lv_obj_align(lbl_int_title, LV_ALIGN_TOP_MID, -52, 1);

    bar_integrity = lv_bar_create(hud_cont);
    lv_obj_set_size(bar_integrity, 110, 10);
    lv_obj_align(bar_integrity, LV_ALIGN_LEFT_MID, 88, 0);
    lv_bar_set_range(bar_integrity, 0, 100);
    lv_bar_set_value(bar_integrity, 100, LV_ANIM_OFF);
    lv_obj_set_style_bg_color(bar_integrity, lv_color_hex(0x1c2e40), 0);
    lv_obj_set_style_bg_color(bar_integrity,
        lv_color_hex(0x00ff88), LV_PART_INDICATOR);
    lv_obj_set_style_radius(bar_integrity, 3, 0);
    lv_obj_set_style_radius(bar_integrity, 3, LV_PART_INDICATOR);

    lbl_integrity_val = lv_label_create(hud_cont);
    lv_label_set_text(lbl_integrity_val, "100%");
    lv_obj_set_style_text_font(lbl_integrity_val, &lv_font_montserrat_10, 0);
    lv_obj_set_style_text_color(lbl_integrity_val, lv_color_hex(0x88ffcc), 0);
    lv_obj_align(lbl_integrity_val, LV_ALIGN_LEFT_MID, 204, 0);

    /* ---- Status (direita) ---- */
    lbl_status = lv_label_create(hud_cont);
    lv_label_set_text(lbl_status, "SISTEMA OK");
    lv_obj_set_style_text_color(lbl_status, lv_color_hex(0x44cc88), 0);
    lv_obj_set_style_text_font(lbl_status, &lv_font_montserrat_10, 0);
    lv_obj_align(lbl_status, LV_ALIGN_RIGHT_MID, -2, 0);
    lv_obj_set_style_text_align(lbl_status, LV_TEXT_ALIGN_RIGHT, 0);
    lv_label_set_long_mode(lbl_status, LV_LABEL_LONG_CLIP);
    lv_obj_set_width(lbl_status, 72);
}

static void create_map(void)
{
    map_cont = lv_obj_create(lv_scr_act());
    lv_obj_set_size(map_cont, SCR_W, MAP_H);
    lv_obj_set_pos(map_cont, 0, MAP_Y);
    lv_obj_set_style_bg_color(map_cont, lv_color_hex(0x0d1520), 0);
    lv_obj_set_style_border_width(map_cont, 0, 0);
    lv_obj_set_style_pad_all(map_cont, 0, 0);
    lv_obj_clear_flag(map_cont, LV_OBJ_FLAG_SCROLLABLE);

    /* ---- Corredores (decorativos) ---- */
    /* Corredor vertical central */
    lv_obj_t *cv = lv_obj_create(map_cont);
    lv_obj_set_size(cv, 14, MAP_H);
    lv_obj_set_pos(cv, 233, 0);
    lv_obj_set_style_bg_color(cv, lv_color_hex(0x151f2e), 0);
    lv_obj_set_style_border_width(cv, 0, 0);
    lv_obj_clear_flag(cv, LV_OBJ_FLAG_SCROLLABLE);

    /* Corredor horizontal central */
    lv_obj_t *ch = lv_obj_create(map_cont);
    lv_obj_set_size(ch, SCR_W, 14);
    lv_obj_set_pos(ch, 0, (MAP_H / 2) - 7);
    lv_obj_set_style_bg_color(ch, lv_color_hex(0x151f2e), 0);
    lv_obj_set_style_border_width(ch, 0, 0);
    lv_obj_clear_flag(ch, LV_OBJ_FLAG_SCROLLABLE);

    /* Linhas de grade (pontilhado simulado por objetos finos) */
    for (int row = 0; row < 2; row++) {
        for (int col = 0; col < 2; col++) {
            int gx = col == 0 ? 4 : 249;
            int gy = row == 0 ? 2 : (MAP_H / 2) + 8;
            lv_obj_t *dot = lv_obj_create(map_cont);
            lv_obj_set_size(dot, 2, 2);
            for (int d = 0; d < 6; d++) {
                lv_obj_t *dotd = lv_obj_create(map_cont);
                lv_obj_set_size(dotd, 1, 1);
                lv_obj_set_pos(dotd, gx + d * 16, gy + 10);
                lv_obj_set_style_bg_color(dotd, lv_color_hex(0x1e2f42), 0);
                lv_obj_set_style_border_width(dotd, 0, 0);
                lv_obj_clear_flag(dotd, LV_OBJ_FLAG_SCROLLABLE);
            }
            (void)dot;
        }
    }

    /* ---- Salas ---- */
    for (int i = 0; i < NUM_ROOMS; i++) {
        Room *r = &rooms[i];
        lv_coord_t rel_y = r->y - MAP_Y; /* relativo ao map_cont */

        room_panel[i] = lv_obj_create(map_cont);
        lv_obj_set_size(room_panel[i], r->w, r->h);
        lv_obj_set_pos(room_panel[i], r->x, rel_y);
        lv_obj_set_style_bg_color(room_panel[i], lv_color_hex(0x162030), 0);
        lv_obj_set_style_border_color(room_panel[i], lv_color_hex(0x2a4060), 0);
        lv_obj_set_style_border_width(room_panel[i], 1, 0);
        lv_obj_set_style_radius(room_panel[i], 4, 0);
        lv_obj_set_style_pad_all(room_panel[i], 5, 0);
        lv_obj_clear_flag(room_panel[i], LV_OBJ_FLAG_SCROLLABLE);

        /* Nome da sala */
        room_lbl_name[i] = lv_label_create(room_panel[i]);
        lv_label_set_text(room_lbl_name[i], r->name);
        lv_obj_set_style_text_color(room_lbl_name[i],
            lv_color_hex(0x5588aa), 0);
        lv_obj_set_style_text_font(room_lbl_name[i],
            &lv_font_montserrat_10, 0);
        lv_obj_align(room_lbl_name[i], LV_ALIGN_TOP_LEFT, 0, 0);

        /* Barra de HP */
        room_hp_bar[i] = lv_bar_create(room_panel[i]);
        lv_obj_set_size(room_hp_bar[i], r->w - 12, 6);
        lv_obj_align(room_hp_bar[i], LV_ALIGN_BOTTOM_MID, 0, -12);
        lv_bar_set_range(room_hp_bar[i], 0, 100);
        lv_bar_set_value(room_hp_bar[i], 100, LV_ANIM_OFF);
        lv_obj_set_style_bg_color(room_hp_bar[i], lv_color_hex(0x1a2a38), 0);
        lv_obj_set_style_bg_color(room_hp_bar[i],
            lv_color_hex(0x00cc66), LV_PART_INDICATOR);
        lv_obj_set_style_radius(room_hp_bar[i], 2, 0);
        lv_obj_set_style_radius(room_hp_bar[i], 2, LV_PART_INDICATOR);

        /* Valor de HP */
        room_lbl_hp[i] = lv_label_create(room_panel[i]);
        lv_label_set_text(room_lbl_hp[i], "HP: 100");
        lv_obj_set_style_text_color(room_lbl_hp[i],
            lv_color_hex(0x44ff88), 0);
        lv_obj_set_style_text_font(room_lbl_hp[i],
            &lv_font_montserrat_10, 0);
        lv_obj_align(room_lbl_hp[i], LV_ALIGN_BOTTOM_LEFT, 0, 0);

        /* Label de evento (centro da sala) */
        room_lbl_ev[i] = lv_label_create(room_panel[i]);
        lv_label_set_text(room_lbl_ev[i], "");
        lv_obj_set_style_text_font(room_lbl_ev[i],
            &lv_font_montserrat_10, 0);
        lv_obj_set_style_text_align(room_lbl_ev[i],
            LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_set_width(room_lbl_ev[i], r->w - 12);
        lv_label_set_long_mode(room_lbl_ev[i], LV_LABEL_LONG_WRAP);
        lv_obj_align(room_lbl_ev[i], LV_ALIGN_CENTER, 0, 4);

        update_room_ui(i);
    }
}

static void create_player(void)
{
    player_icon = lv_obj_create(map_cont);
    lv_obj_set_size(player_icon, PLR_SIZE, PLR_SIZE);
    lv_obj_set_style_bg_color(player_icon, lv_color_hex(0xffffff), 0);
    lv_obj_set_style_border_color(player_icon, lv_color_hex(0x00ffc8), 0);
    lv_obj_set_style_border_width(player_icon, 2, 0);
    lv_obj_set_style_radius(player_icon, 2, 0);
    lv_obj_set_style_shadow_width(player_icon, 8, 0);
    lv_obj_set_style_shadow_color(player_icon, lv_color_hex(0x00ffc8), 0);
    lv_obj_set_style_shadow_opa(player_icon, LV_OPA_50, 0);
    lv_obj_clear_flag(player_icon, LV_OBJ_FLAG_SCROLLABLE);
    update_player_pos();
}

static void start_timers(void)
{
    if (tmr_move  == NULL) tmr_move  = lv_timer_create(tmr_move_cb,  MOVE_TICK_MS, NULL);
    if (tmr_clock == NULL) tmr_clock = lv_timer_create(tmr_clock_cb, CLOCK_TICK_MS, NULL);
    if (tmr_event == NULL) tmr_event = lv_timer_create(tmr_event_cb, EVENT_MIN_MS, NULL);
    if (tmr_drain == NULL) tmr_drain = lv_timer_create(tmr_drain_cb, DRAIN_TICK_MS, NULL);
}

static void stop_and_del_timers(void)
{
    if (tmr_move)  { lv_timer_del(tmr_move);  tmr_move  = NULL; }
    if (tmr_clock) { lv_timer_del(tmr_clock); tmr_clock = NULL; }
    if (tmr_event) { lv_timer_del(tmr_event); tmr_event = NULL; }
    if (tmr_drain) { lv_timer_del(tmr_drain); tmr_drain = NULL; }
}

/* ============================================================
 *  CALLBACKS DE TIMER
 * ============================================================*/

/** Timer de movimentação — ~60fps */
static void tmr_move_cb(lv_timer_t *t)
{
    (void)t;
    if (!game_running || dlg_overlay) return;

    if (keys.up)    plr.y -= PLR_SPEED;
    if (keys.down)  plr.y += PLR_SPEED;
    if (keys.left)  plr.x -= PLR_SPEED;
    if (keys.right) plr.x += PLR_SPEED;

    /* Limites da tela */
    if (plr.x < 0)               plr.x = 0;
    if (plr.x > SCR_W - PLR_SIZE) plr.x = SCR_W - PLR_SIZE;
    if (plr.y < MAP_Y)            plr.y = MAP_Y;
    if (plr.y > SCR_H - PLR_SIZE) plr.y = SCR_H - PLR_SIZE;

    update_player_pos();
}

/** Timer do relógio do jogo */
static void tmr_clock_cb(lv_timer_t *t)
{
    (void)t;
    if (!game_running) return;

    clock_ticks++;

    /* Mapeia ticks reais para horas de jogo */
    int total_game_min = (clock_ticks * GAME_TOTAL_MINUTES) / GAME_TOTAL_TICKS;
    game_hour = 8  + (total_game_min / 60);
    game_min  = total_game_min % 60;

    update_hud_ui();

    if (game_hour >= 18) {
        do_game_over(true);
    }
}

/** Timer de spawn de eventos */
static void tmr_event_cb(lv_timer_t *t)
{
    if (!game_running || dlg_overlay) return;

    /* Seleciona sala sem evento ativo */
    int candidates[NUM_ROOMS];
    int nc = 0;
    for (int i = 0; i < NUM_ROOMS; i++) {
        if (rooms[i].event == EV_NONE) candidates[nc++] = i;
    }
    if (nc == 0) return;

    int room = candidates[rand() % nc];

    /* Probabilidade: 40% verde, 30% amarelo, 30% vermelho */
    int r = rand() % 10;
    EventType ev;
    if      (r < 4) ev = EV_ROUTINE;
    else if (r < 7) ev = EV_ANOMALY;
    else            ev = EV_CRITICAL;

    spawn_event(room, ev);

    /* Reagenda com intervalo aleatório */
    uint32_t next = EVENT_MIN_MS +
                    (uint32_t)(rand() % (EVENT_MAX_MS - EVENT_MIN_MS));
    lv_timer_set_period(t, next);
}

/** Timer de drenagem de HP por evento ignorado */
static void tmr_drain_cb(lv_timer_t *t)
{
    (void)t;
    if (!game_running) return;

    for (int i = 0; i < NUM_ROOMS; i++) {
        if (rooms[i].event == EV_NONE) continue;

        int drain = (rooms[i].event == EV_ROUTINE)  ? DRAIN_ROUTINE  :
                    (rooms[i].event == EV_ANOMALY)   ? DRAIN_ANOMALY  :
                                                       DRAIN_CRITICAL;
        rooms[i].health -= drain;
        if (rooms[i].health < 0) rooms[i].health = 0;
        update_room_ui(i);
    }

    recalc_integrity();
    update_hud_ui();

    if (net_integrity <= 0) {
        do_game_over(false);
    }
}

/* ============================================================
 *  LÓGICA DE JOGO
 * ============================================================*/

/** Spawna um evento em uma sala e atualiza UI + HUD */
static void spawn_event(int room_idx, EventType ev)
{
    rooms[room_idx].event = ev;
    update_room_ui(room_idx);

    /* HUD: alerta visual */
    char buf[64];
    snprintf(buf, sizeof(buf), "%s: %s!",
             ev_label(ev), rooms[room_idx].name);
    set_status_msg(buf, ev_color(ev));

    if (ev == EV_CRITICAL) {
        /* Piscar fundo do HUD para indicar emergência */
        lv_obj_set_style_bg_color(hud_cont, lv_color_hex(0x2a0808), 0);
    }
}

/** Resolve o evento ativo numa sala */
static void resolve_room(int room_idx, bool nfc)
{
    if (rooms[room_idx].event == EV_NONE) return;

    /* NFC recupera mais HP */
    int hp_gain = nfc ? 30 : 15;
    rooms[room_idx].health += hp_gain;
    if (rooms[room_idx].health > 100) rooms[room_idx].health = 100;
    rooms[room_idx].event = EV_NONE;

    update_room_ui(room_idx);
    recalc_integrity();
    update_hud_ui();

    char buf[48];
    snprintf(buf, sizeof(buf), "%s restaurada! +%d HP",
             rooms[room_idx].name, hp_gain);
    set_status_msg(buf, lv_color_hex(0x44ff88));
    lv_obj_set_style_bg_color(hud_cont, lv_color_hex(0x0b1624), 0);
}

/** Recalcula a integridade global da rede */
static void recalc_integrity(void)
{
    int total = 0;
    for (int i = 0; i < NUM_ROOMS; i++) total += rooms[i].health;
    net_integrity = total / NUM_ROOMS;
}

/** Retorna o índice da sala onde o jogador está (-1 se no corredor) */
static int get_player_room(void)
{
    for (int i = 0; i < NUM_ROOMS; i++) {
        Room *r = &rooms[i];
        /* Colisão AABB: jogador (ponto superior-esquerdo + tamanho) vs sala */
        if (plr.x + PLR_SIZE > r->x      &&
            plr.x            < r->x + r->w &&
            plr.y + PLR_SIZE > r->y      &&
            plr.y            < r->y + r->h) {
            return i;
        }
    }
    return -1;
}

/* ============================================================
 *  ATUALIZAÇÃO DE UI
 * ============================================================*/

/** Atualiza visualmente uma sala com base no seu estado atual */
static void update_room_ui(int idx)
{
    Room *r = &rooms[idx];

    /* Cor de fundo do painel */
    lv_color_t bg_color =
        (r->event == EV_NONE)     ? lv_color_hex(0x162030) :
        (r->event == EV_ROUTINE)  ? lv_color_hex(0x0a2012) :
        (r->event == EV_ANOMALY)  ? lv_color_hex(0x221800) :
                                    lv_color_hex(0x280808);

    lv_obj_set_style_bg_color(room_panel[idx], bg_color, 0);
    lv_obj_set_style_border_color(room_panel[idx], ev_color(r->event), 0);
    lv_obj_set_style_border_width(room_panel[idx],
        (r->event == EV_NONE) ? 1 : 2, 0);

    /* Label de evento */
    if (r->event == EV_NONE) {
        lv_label_set_text(room_lbl_ev[idx], "");
    } else {
        char buf[48];
        if (r->event == EV_CRITICAL) {
            snprintf(buf, sizeof(buf), "!! RANSOMWARE !!\n[N] NFC Scan");
        } else {
            snprintf(buf, sizeof(buf), "[%s]\n[SPACE]", ev_label(r->event));
        }
        lv_label_set_text(room_lbl_ev[idx], buf);
        lv_obj_set_style_text_color(room_lbl_ev[idx], ev_color(r->event), 0);
    }

    /* Barra de HP */
    lv_bar_set_value(room_hp_bar[idx], r->health, LV_ANIM_ON);
    lv_color_t hp_bar_color =
        (r->health > 60) ? lv_color_hex(0x00cc66) :
        (r->health > 30) ? lv_color_hex(0xffcc00) :
                           lv_color_hex(0xff3333);
    lv_obj_set_style_bg_color(room_hp_bar[idx],
        hp_bar_color, LV_PART_INDICATOR);

    /* Label HP */
    char hp_buf[16];
    snprintf(hp_buf, sizeof(hp_buf), "HP: %d", r->health);
    lv_label_set_text(room_lbl_hp[idx], hp_buf);
    lv_obj_set_style_text_color(room_lbl_hp[idx], hp_bar_color, 0);
}

/** Atualiza o HUD (relógio, barra, status de integridade) */
static void update_hud_ui(void)
{
    /* Relógio */
    char clock_buf[8];
    snprintf(clock_buf, sizeof(clock_buf), "%02d:%02d", game_hour, game_min);
    lv_label_set_text(lbl_time, clock_buf);

    /* Barra de integridade */
    lv_bar_set_value(bar_integrity, net_integrity, LV_ANIM_ON);

    char int_buf[8];
    snprintf(int_buf, sizeof(int_buf), "%d%%", net_integrity);
    lv_label_set_text(lbl_integrity_val, int_buf);

    /* Cor dinâmica da barra */
    lv_color_t bar_color =
        (net_integrity > 60) ? lv_color_hex(0x00ff88) :
        (net_integrity > 30) ? lv_color_hex(0xffcc00) :
                               lv_color_hex(0xff4444);
    lv_obj_set_style_bg_color(bar_integrity, bar_color, LV_PART_INDICATOR);
    lv_obj_set_style_text_color(lbl_integrity_val, bar_color, 0);
}

/** Atualiza a posição do ícone do jogador no map_cont */
static void update_player_pos(void)
{
    /* map_cont começa em MAP_Y, então a posição relativa é y - MAP_Y */
    lv_obj_set_pos(player_icon, plr.x, plr.y - MAP_Y);
}

/** Atualiza o label de status do HUD */
static void set_status_msg(const char *msg, lv_color_t color)
{
    lv_label_set_text(lbl_status, msg);
    lv_obj_set_style_text_color(lbl_status, color, 0);
}

/* ============================================================
 *  DIALOG DE OVERLAY
 * ============================================================*/

/** Callback do botão OK do dialog */
static void dlg_close_cb(lv_event_t *e)
{
    (void)e;
    if (dlg_overlay) {
        lv_obj_del(dlg_overlay);
        dlg_overlay = NULL;
    }
    /* Primeira abertura: dialog de boas-vindas fecha → jogo começa */
    game_running = true;
}

/**
 * Exibe um dialog modal centralizado.
 * @param is_game_end Se true, mostra "Reiniciar" em vez de "OK"
 */
static void show_dialog(const char *title, const char *msg, bool is_game_end)
{
    if (dlg_overlay) {
        lv_obj_del(dlg_overlay);
        dlg_overlay = NULL;
    }

    dlg_overlay = lv_obj_create(lv_scr_act());
    lv_obj_set_size(dlg_overlay, 250, 160);
    lv_obj_center(dlg_overlay);
    lv_obj_set_style_bg_color(dlg_overlay, lv_color_hex(0x0c1826), 0);
    lv_obj_set_style_border_color(dlg_overlay, lv_color_hex(0x00ffc8), 0);
    lv_obj_set_style_border_width(dlg_overlay, 2, 0);
    lv_obj_set_style_radius(dlg_overlay, 6, 0);
    lv_obj_set_style_pad_all(dlg_overlay, 10, 0);
    lv_obj_set_style_shadow_width(dlg_overlay, 20, 0);
    lv_obj_set_style_shadow_color(dlg_overlay, lv_color_hex(0x00ffc8), 0);
    lv_obj_set_style_shadow_opa(dlg_overlay, LV_OPA_30, 0);
    lv_obj_clear_flag(dlg_overlay, LV_OBJ_FLAG_SCROLLABLE);

    /* Título */
    lv_obj_t *lbl_title = lv_label_create(dlg_overlay);
    lv_label_set_text(lbl_title, title);
    lv_obj_set_style_text_color(lbl_title, lv_color_hex(0x00ffc8), 0);
    lv_obj_set_style_text_font(lbl_title, &lv_font_montserrat_14, 0);
    lv_obj_align(lbl_title, LV_ALIGN_TOP_MID, 0, 0);

    /* Separador */
    lv_obj_t *sep = lv_obj_create(dlg_overlay);
    lv_obj_set_size(sep, 220, 1);
    lv_obj_set_style_bg_color(sep, lv_color_hex(0x1e3a50), 0);
    lv_obj_set_style_border_width(sep, 0, 0);
    lv_obj_align(sep, LV_ALIGN_TOP_MID, 0, 20);
    lv_obj_clear_flag(sep, LV_OBJ_FLAG_SCROLLABLE);

    /* Mensagem */
    lv_obj_t *lbl_msg = lv_label_create(dlg_overlay);
    lv_label_set_text(lbl_msg, msg);
    lv_obj_set_style_text_color(lbl_msg, lv_color_hex(0xaaccdd), 0);
    lv_obj_set_style_text_font(lbl_msg, &lv_font_montserrat_10, 0);
    lv_obj_set_width(lbl_msg, 225);
    lv_label_set_long_mode(lbl_msg, LV_LABEL_LONG_WRAP);
    lv_obj_align(lbl_msg, LV_ALIGN_TOP_MID, 0, 26);

    /* Botão */
    lv_obj_t *btn = lv_btn_create(dlg_overlay);
    lv_obj_set_size(btn, 100, 26);
    lv_obj_align(btn, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_set_style_bg_color(btn, lv_color_hex(0x006644), 0);
    lv_obj_set_style_radius(btn, 4, 0);
    lv_obj_set_style_border_width(btn, 0, 0);
    lv_obj_add_event_cb(btn,
        is_game_end ? (lv_event_cb_t)do_restart : dlg_close_cb,
        LV_EVENT_CLICKED, NULL);

    lv_obj_t *btn_lbl = lv_label_create(btn);
    lv_label_set_text(btn_lbl, is_game_end ? "Reiniciar [R]" : "OK [SPACE]");
    lv_obj_set_style_text_font(btn_lbl, &lv_font_montserrat_10, 0);
    lv_obj_center(btn_lbl);
}

/** Finaliza o jogo e exibe tela de resultado */
static void do_game_over(bool victory)
{
    game_running = false;
    stop_and_del_timers();
    memset(&keys, 0, sizeof(keys));

    char msg[160];
    if (victory) {
        snprintf(msg, sizeof(msg),
                 "Expeidiente encerrado com sucesso!\n\n"
                 "Integridade final da rede: %d%%\n\n"
                 "Voce protegeu a empresa de todos\n"
                 "os ataques ciberneticos!\n\n"
                 "Pontuacao: %d pontos",
                 net_integrity,
                 net_integrity * 10 + (18 - game_hour) * 50);
    } else {
        snprintf(msg, sizeof(msg),
                 "FALHA CRITICA NO SISTEMA!\n\n"
                 "A integridade da rede chegou a ZERO.\n"
                 "A empresa foi comprometida por Ransomware.\n\n"
                 "Analise os incidentes ignorados\n"
                 "e tente novamente.");
    }

    show_dialog(
        victory ? "MISSAO CONCLUIDA!" : "GAME OVER",
        msg,
        true
    );
}

/** Reinicia o jogo */
static void do_restart(void)
{
    stop_and_del_timers();
    memset(&keys, 0, sizeof(keys));
    cybersec_start();
}

/* ============================================================
 *  HELPERS
 * ============================================================*/

static lv_color_t ev_color(EventType ev)
{
    switch (ev) {
        case EV_ROUTINE:  return lv_color_hex(0x00cc55);
        case EV_ANOMALY:  return lv_color_hex(0xffaa00);
        case EV_CRITICAL: return lv_color_hex(0xff2222);
        default:          return lv_color_hex(0x2a4060);
    }
}

static const char *ev_label(EventType ev)
{
    switch (ev) {
        case EV_ROUTINE:  return "TAREFA";
        case EV_ANOMALY:  return "ANOMALIA";
        case EV_CRITICAL: return "RANSOMWARE";
        default:          return "";
    }
}