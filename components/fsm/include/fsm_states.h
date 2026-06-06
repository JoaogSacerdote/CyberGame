#pragma once

#ifdef __cplusplus
extern "C" {
#endif

/* Estados macro da FSM do jogo. Sub-estados de gameplay (TERMINAL_ABERTO,
 * WAITING_CARD, ACTION_LOCK, SYSTEM_DEPLOY) sao internos ao gameplay e ficam
 * em fsm.c — nao precisam aparecer no enum publico. */
typedef enum {
    GAME_STATE_SPLASH = 0,
    GAME_STATE_MENU,
    GAME_STATE_GAMEPLAY,       /* placeholder na Etapa A; sub-FSM completa na Etapa C+ */
    GAME_STATE_PAUSE,
    GAME_STATE_GAME_OVER,
    GAME_STATE_RANKING_VIEW,
    GAME_STATE_CREDITOS,
    GAME_STATE_MAX,
} game_state_t;

#ifdef __cplusplus
}
#endif
