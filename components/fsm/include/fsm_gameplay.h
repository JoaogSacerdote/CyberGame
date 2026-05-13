#pragma once

#ifdef __cplusplus
extern "C" {
#endif

/* Sub-estados internos a GAME_STATE_GAMEPLAY. Encapsulam o loop de defesa
 * do expediente: explorar -> abrir terminal -> escanear carta -> trava de
 * acao -> deploy do sistema -> volta a explorar. Outros estados macro
 * (MENU/PAUSE/SPLASH) nao tem sub-estados.
 */
typedef enum {
    GAMEPLAY_SUB_EXPLORANDO = 0,
    GAMEPLAY_SUB_TERMINAL_ABERTO,
    GAMEPLAY_SUB_WAITING_CARD,
    GAMEPLAY_SUB_ACTION_LOCK,
    GAMEPLAY_SUB_SYSTEM_DEPLOY,
    GAMEPLAY_SUB_MAX,
} gameplay_substate_t;

gameplay_substate_t fsm_get_gameplay_substate(void);
const char         *fsm_gameplay_substate_name(gameplay_substate_t s);

#ifdef __cplusplus
}
#endif
