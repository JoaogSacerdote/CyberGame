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

/* Qual sala o player esta. Spawn inicial e SALA_RECEPCAO; gatilhos
 * AREA_PORTA_* na propria sala disparam fsm_set_gameplay_sala(). */
typedef enum {
    GAMEPLAY_SALA_RECEPCAO = 0,
    GAMEPLAY_SALA_EMPRESA,
    GAMEPLAY_SALA_MAX,
} gameplay_sala_t;

gameplay_substate_t fsm_get_gameplay_substate(void);
const char         *fsm_gameplay_substate_name(gameplay_substate_t s);

gameplay_sala_t     fsm_get_gameplay_sala(void);
void                fsm_set_gameplay_sala(gameplay_sala_t sala);
const char         *fsm_gameplay_sala_name(gameplay_sala_t s);

/* Sala de onde o player veio na ultima troca. Usado pelas telas pra
 * decidir o ponto de spawn (ex: voltar da Empresa -> nascer na porta). */
gameplay_sala_t     fsm_get_gameplay_sala_prev(void);

#ifdef __cplusplus
}
#endif
