#include "fsm.h"
#include "fsm_gameplay.h"
#include "button_hal.h"
#include "gamestate.h"
#include "test_runner.h"

static void make_button_evt(fsm_event_t *evt, button_id_t btn, button_state_t state) {
    evt->kind = FSM_EVT_BUTTON;
    evt->payload.button.id    = (uint8_t)btn;
    evt->payload.button.state = (uint8_t)state;
}

static void make_tick_evt(fsm_event_t *evt, uint32_t dt_ms) {
    evt->kind = FSM_EVT_TICK;
    evt->payload.tick.dt_ms = dt_ms;
}

/* (a) fsm_init coloca o jogo em SPLASH. */
static void test_init_starts_in_splash(void) {
    fsm_init();
    TEST_EQ_INT(fsm_get_state(), GAME_STATE_SPLASH);
    TEST_EQ_INT(fsm_get_gameplay_sala(), GAMEPLAY_SALA_RECEPCAO);
    TEST_EQ_INT(fsm_get_gameplay_substate(), GAMEPLAY_SUB_EXPLORANDO);
}

/* (b) set_state SPLASH -> MENU funciona. */
static void test_splash_to_menu(void) {
    fsm_init();
    fsm_set_state(GAME_STATE_MENU);
    TEST_EQ_INT(fsm_get_state(), GAME_STATE_MENU);
}

/* (c) MENU -> GAMEPLAY reseta sub-FSM pra EXPLORANDO e sala pra RECEPCAO,
 *     mesmo que sala estivesse poluida. */
static void test_menu_to_gameplay_resets_sub(void) {
    fsm_init();
    fsm_set_state(GAME_STATE_MENU);
    fsm_set_gameplay_sala(GAMEPLAY_SALA_EMPRESA); /* polui antes de entrar */
    fsm_set_state(GAME_STATE_GAMEPLAY);
    TEST_EQ_INT(fsm_get_state(), GAME_STATE_GAMEPLAY);
    TEST_EQ_INT(fsm_get_gameplay_substate(), GAMEPLAY_SUB_EXPLORANDO);
    TEST_EQ_INT(fsm_get_gameplay_sala(), GAMEPLAY_SALA_RECEPCAO);
}

/* (d) GAMEPLAY + START -> PAUSE. */
static void test_gameplay_start_pauses(void) {
    fsm_init();
    fsm_set_state(GAME_STATE_GAMEPLAY);
    fsm_event_t evt;
    make_button_evt(&evt, BTN_START, BTN_PRESSED);
    fsm_handle_event(&evt);
    TEST_EQ_INT(fsm_get_state(), GAME_STATE_PAUSE);
}

/* (e) PAUSE + START retoma GAMEPLAY preservando sala (regressao do bug
 *     "pause preserva estado"). Hoje a sala e preservada mas o sub-estado
 *     ainda reseta — esse teste deixa o ponto de partida explicito pro
 *     fix futuro. */
static void test_pause_start_resumes_keeping_sala(void) {
    fsm_init();
    fsm_set_state(GAME_STATE_GAMEPLAY);
    fsm_set_gameplay_sala(GAMEPLAY_SALA_EMPRESA);

    fsm_event_t evt;
    make_button_evt(&evt, BTN_START, BTN_PRESSED);
    fsm_handle_event(&evt); /* -> PAUSE */
    TEST_EQ_INT(fsm_get_state(), GAME_STATE_PAUSE);

    fsm_handle_event(&evt); /* -> GAMEPLAY */
    TEST_EQ_INT(fsm_get_state(), GAME_STATE_GAMEPLAY);
    TEST_EQ_INT(fsm_get_gameplay_sala(), GAMEPLAY_SALA_EMPRESA);
}

/* (f) Loop completo do sub-FSM:
 *     EXPLORANDO -Y-> TERMINAL_ABERTO -A-> WAITING_CARD -X-> ACTION_LOCK
 *     -tick 1500ms-> SYSTEM_DEPLOY -tick 4000ms-> EXPLORANDO.
 */
static void test_sub_fsm_full_loop(void) {
    fsm_init();
    fsm_set_state(GAME_STATE_GAMEPLAY);
    fsm_set_player_at_equipment(true);  /* pre-requisito de Y em EXPLORANDO */
    fsm_event_t evt;

    make_button_evt(&evt, BTN_Y, BTN_PRESSED);
    fsm_handle_event(&evt);
    TEST_EQ_INT(fsm_get_gameplay_substate(), GAMEPLAY_SUB_TERMINAL_ABERTO);

    make_button_evt(&evt, BTN_A, BTN_PRESSED);
    fsm_handle_event(&evt);
    TEST_EQ_INT(fsm_get_gameplay_substate(), GAMEPLAY_SUB_WAITING_CARD);

    make_button_evt(&evt, BTN_X, BTN_PRESSED);
    fsm_handle_event(&evt);
    TEST_EQ_INT(fsm_get_gameplay_substate(), GAMEPLAY_SUB_ACTION_LOCK);

    make_tick_evt(&evt, 1500); /* >= FSM_ACTION_LOCK_MS */
    fsm_handle_event(&evt);
    TEST_EQ_INT(fsm_get_gameplay_substate(), GAMEPLAY_SUB_SYSTEM_DEPLOY);

    make_tick_evt(&evt, 4000); /* >= FSM_SYSTEM_DEPLOY_MS */
    fsm_handle_event(&evt);
    TEST_EQ_INT(fsm_get_gameplay_substate(), GAMEPLAY_SUB_EXPLORANDO);
}

/* (g) BUG REGRESSION: PAUSE deve preservar o sub-estado de gameplay.
 *     Antes do fix: sub-FSM reseta pra EXPLORANDO ao retomar.
 *     Depois do fix: sub-FSM (ex: WAITING_CARD) sobrevive. */
static void test_pause_preserves_substate(void) {
    fsm_init();
    fsm_set_state(GAME_STATE_GAMEPLAY);
    fsm_set_player_at_equipment(true);

    fsm_event_t evt;
    make_button_evt(&evt, BTN_Y, BTN_PRESSED);     /* EXPLORANDO -> TERMINAL_ABERTO */
    fsm_handle_event(&evt);
    make_button_evt(&evt, BTN_A, BTN_PRESSED);     /* -> WAITING_CARD */
    fsm_handle_event(&evt);
    TEST_EQ_INT(fsm_get_gameplay_substate(), GAMEPLAY_SUB_WAITING_CARD);

    make_button_evt(&evt, BTN_START, BTN_PRESSED); /* -> PAUSE */
    fsm_handle_event(&evt);
    TEST_EQ_INT(fsm_get_state(), GAME_STATE_PAUSE);

    fsm_handle_event(&evt);                         /* -> GAMEPLAY (resume) */
    TEST_EQ_INT(fsm_get_state(), GAME_STATE_GAMEPLAY);
    TEST_EQ_INT(fsm_get_gameplay_substate(), GAMEPLAY_SUB_WAITING_CARD);
}

/* (h) BUG REGRESSION: PAUSE deve preservar phase_ms acumulado em sub-estados
 *     temporizados (ACTION_LOCK / SYSTEM_DEPLOY). Antes do fix, retomar reseta
 *     o timer e o player tem que esperar o periodo cheio de novo. */
static void test_pause_preserves_phase_ms(void) {
    fsm_init();
    fsm_set_state(GAME_STATE_GAMEPLAY);
    fsm_set_player_at_equipment(true);

    fsm_event_t evt;
    make_button_evt(&evt, BTN_Y, BTN_PRESSED);     fsm_handle_event(&evt);
    make_button_evt(&evt, BTN_A, BTN_PRESSED);     fsm_handle_event(&evt);
    make_button_evt(&evt, BTN_X, BTN_PRESSED);     fsm_handle_event(&evt);
    TEST_EQ_INT(fsm_get_gameplay_substate(), GAMEPLAY_SUB_ACTION_LOCK);

    make_tick_evt(&evt, 1000);                      /* parcial: faltam 500ms */
    fsm_handle_event(&evt);
    TEST_EQ_INT(fsm_get_gameplay_substate(), GAMEPLAY_SUB_ACTION_LOCK);

    make_button_evt(&evt, BTN_START, BTN_PRESSED); fsm_handle_event(&evt); /* PAUSE */
    TEST_EQ_INT(fsm_get_state(), GAME_STATE_PAUSE);
    fsm_handle_event(&evt);                                                /* RESUME */
    TEST_EQ_INT(fsm_get_state(), GAME_STATE_GAMEPLAY);
    TEST_EQ_INT(fsm_get_gameplay_substate(), GAMEPLAY_SUB_ACTION_LOCK);

    /* 1000ms acumulados antes da pause + 600ms agora = 1600ms >= 1500ms.
     * Se phase_ms preservar, transiciona pra SYSTEM_DEPLOY. */
    make_tick_evt(&evt, 600);
    fsm_handle_event(&evt);
    TEST_EQ_INT(fsm_get_gameplay_substate(), GAMEPLAY_SUB_SYSTEM_DEPLOY);
}

/* (i) BUG REGRESSION: Y em EXPLORANDO sem player-at-equipment NAO deve
 *     transicionar para TERMINAL_ABERTO. */
static void test_y_without_equipment_does_nothing(void) {
    fsm_init();
    fsm_set_state(GAME_STATE_GAMEPLAY);
    TEST_ASSERT(fsm_get_player_at_equipment() == false);  /* default */

    fsm_event_t evt;
    make_button_evt(&evt, BTN_Y, BTN_PRESSED);
    fsm_handle_event(&evt);
    TEST_EQ_INT(fsm_get_gameplay_substate(), GAMEPLAY_SUB_EXPLORANDO);
}

/* (j) Y em EXPLORANDO COM player-at-equipment transiciona para TERMINAL_ABERTO. */
static void test_y_with_equipment_opens_terminal(void) {
    fsm_init();
    fsm_set_state(GAME_STATE_GAMEPLAY);
    fsm_set_player_at_equipment(true);
    TEST_ASSERT(fsm_get_player_at_equipment() == true);

    fsm_event_t evt;
    make_button_evt(&evt, BTN_Y, BTN_PRESSED);
    fsm_handle_event(&evt);
    TEST_EQ_INT(fsm_get_gameplay_substate(), GAMEPLAY_SUB_TERMINAL_ABERTO);
}

/* (k) Flag reseta em uma run nova de gameplay (MENU -> GAMEPLAY). Caso
 *     contrario, o player retornaria ao gameplay com flag preservada da
 *     run anterior e Y dispararia ali no spawn. */
static void test_player_at_equipment_resets_on_new_run(void) {
    fsm_init();
    fsm_set_state(GAME_STATE_MENU);
    fsm_set_state(GAME_STATE_GAMEPLAY);
    fsm_set_player_at_equipment(true);          /* poluindo */
    fsm_set_state(GAME_STATE_MENU);             /* saida */
    fsm_set_state(GAME_STATE_GAMEPLAY);         /* nova run */
    TEST_ASSERT(fsm_get_player_at_equipment() == false);

    /* E o Y nao deve transicionar imediatamente. */
    fsm_event_t evt;
    make_button_evt(&evt, BTN_Y, BTN_PRESSED);
    fsm_handle_event(&evt);
    TEST_EQ_INT(fsm_get_gameplay_substate(), GAMEPLAY_SUB_EXPLORANDO);
}

/* Helper: leva a FSM ate o sub-estado WAITING_CARD via caminho normal. */
static void navigate_to_waiting_card(void) {
    fsm_set_state(GAME_STATE_GAMEPLAY);
    fsm_set_player_at_equipment(true);
    fsm_event_t e;
    make_button_evt(&e, BTN_Y, BTN_PRESSED); fsm_handle_event(&e); /* -> TERMINAL_ABERTO */
    make_button_evt(&e, BTN_A, BTN_PRESSED); fsm_handle_event(&e); /* -> WAITING_CARD */
}

/* (l) Y em WAITING_CARD perde uma vida e MANTEM o sub-estado. */
static void test_wrong_card_decrements_vida_and_stays(void) {
    fsm_init();
    gamestate_init();
    navigate_to_waiting_card();
    TEST_EQ_INT(fsm_get_gameplay_substate(), GAMEPLAY_SUB_WAITING_CARD);
    TEST_EQ_INT(gamestate_get_vidas(), 3);

    fsm_event_t e;
    make_button_evt(&e, BTN_Y, BTN_PRESSED);
    fsm_handle_event(&e);

    TEST_EQ_INT(gamestate_get_vidas(), 2);
    TEST_EQ_INT(fsm_get_gameplay_substate(), GAMEPLAY_SUB_WAITING_CARD); /* sub nao mudou */
}

/* (m) Spam de Y em WAITING_CARD trava as vidas em 0 (sem underflow). */
static void test_wrong_card_spam_floor_at_zero(void) {
    fsm_init();
    gamestate_init();
    navigate_to_waiting_card();

    fsm_event_t e;
    make_button_evt(&e, BTN_Y, BTN_PRESSED);
    for (int i = 0; i < 10; ++i) fsm_handle_event(&e);

    TEST_EQ_INT(gamestate_get_vidas(), 0);
    TEST_EQ_INT(fsm_get_gameplay_substate(), GAMEPLAY_SUB_WAITING_CARD);
}

/* (n) X em WAITING_CARD (carta correta mock) NAO mexe em vidas. */
static void test_right_card_does_not_change_vidas(void) {
    fsm_init();
    gamestate_init();
    navigate_to_waiting_card();
    TEST_EQ_INT(gamestate_get_vidas(), 3);

    fsm_event_t e;
    make_button_evt(&e, BTN_X, BTN_PRESSED);
    fsm_handle_event(&e);

    TEST_EQ_INT(gamestate_get_vidas(), 3);
    TEST_EQ_INT(fsm_get_gameplay_substate(), GAMEPLAY_SUB_ACTION_LOCK);
}

/* (o) Apos 3 cartas erradas (vidas 3->0), FSM transiciona pra GAME_OVER
 *     automaticamente, sem intervencao externa. */
static void test_vidas_zero_triggers_game_over(void) {
    fsm_init();
    gamestate_init();
    navigate_to_waiting_card();

    fsm_event_t e;
    make_button_evt(&e, BTN_Y, BTN_PRESSED);
    fsm_handle_event(&e); /* vidas 3 -> 2 */
    fsm_handle_event(&e); /* vidas 2 -> 1 */
    fsm_handle_event(&e); /* vidas 1 -> 0 -> GAME_OVER */

    TEST_EQ_INT(fsm_get_state(), GAME_STATE_GAME_OVER);
    TEST_EQ_INT(gamestate_get_vidas(), 0);
}

/* (p) A em GAME_OVER = Tentar Novamente -> GAMEPLAY com sub-FSM resetada
 *     (sala/sub/phase/equipment voltam ao padrao). Reset de vidas/clock
 *     e responsabilidade do engine — nao validado aqui. */
static void test_game_over_a_retries(void) {
    fsm_init();
    gamestate_init();
    navigate_to_waiting_card();
    fsm_set_gameplay_sala(GAMEPLAY_SALA_EMPRESA);  /* polui */
    fsm_set_player_at_equipment(true);

    /* Forca game over manualmente. */
    fsm_set_state(GAME_STATE_GAME_OVER);
    TEST_EQ_INT(fsm_get_state(), GAME_STATE_GAME_OVER);

    fsm_event_t e;
    make_button_evt(&e, BTN_A, BTN_PRESSED);
    fsm_handle_event(&e);

    TEST_EQ_INT(fsm_get_state(), GAME_STATE_GAMEPLAY);
    TEST_EQ_INT(fsm_get_gameplay_substate(), GAMEPLAY_SUB_EXPLORANDO);
    TEST_EQ_INT(fsm_get_gameplay_sala(), GAMEPLAY_SALA_RECEPCAO);
    TEST_ASSERT(fsm_get_player_at_equipment() == false);
}

/* (q) B em GAME_OVER = Sair -> MENU. */
static void test_game_over_b_returns_to_menu(void) {
    fsm_init();
    fsm_set_state(GAME_STATE_GAME_OVER);

    fsm_event_t e;
    make_button_evt(&e, BTN_B, BTN_PRESSED);
    fsm_handle_event(&e);

    TEST_EQ_INT(fsm_get_state(), GAME_STATE_MENU);
}

/* (r) 30s ocioso em GAME_OVER (sem botao) -> SPLASH. */
static void test_game_over_timeout_returns_to_splash(void) {
    fsm_init();
    fsm_set_state(GAME_STATE_GAME_OVER);

    fsm_event_t e;
    make_tick_evt(&e, 29000);
    fsm_handle_event(&e);
    TEST_EQ_INT(fsm_get_state(), GAME_STATE_GAME_OVER);  /* ainda nao */

    make_tick_evt(&e, 1000);  /* total 30000 = FSM_TIMEOUT_TELA_FINAL_MS */
    fsm_handle_event(&e);
    TEST_EQ_INT(fsm_get_state(), GAME_STATE_SPLASH);
}

int main(void) {
    TEST_RUN(test_init_starts_in_splash);
    TEST_RUN(test_splash_to_menu);
    TEST_RUN(test_menu_to_gameplay_resets_sub);
    TEST_RUN(test_gameplay_start_pauses);
    TEST_RUN(test_pause_start_resumes_keeping_sala);
    TEST_RUN(test_sub_fsm_full_loop);
    TEST_RUN(test_pause_preserves_substate);
    TEST_RUN(test_pause_preserves_phase_ms);
    TEST_RUN(test_y_without_equipment_does_nothing);
    TEST_RUN(test_y_with_equipment_opens_terminal);
    TEST_RUN(test_player_at_equipment_resets_on_new_run);
    TEST_RUN(test_wrong_card_decrements_vida_and_stays);
    TEST_RUN(test_wrong_card_spam_floor_at_zero);
    TEST_RUN(test_right_card_does_not_change_vidas);
    TEST_RUN(test_vidas_zero_triggers_game_over);
    TEST_RUN(test_game_over_a_retries);
    TEST_RUN(test_game_over_b_returns_to_menu);
    TEST_RUN(test_game_over_timeout_returns_to_splash);
    TEST_SUMMARY();
}
