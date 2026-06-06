#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Estado de jogo persistente durante uma run de gameplay.
 *
 * Por enquanto contem apenas o **relogio do expediente** — mapeia o
 * tempo real decorrido para o horario in-game (08:00 -> 18:00). Sera
 * estendido depois com vidas, ataques ativos, score, etc.
 *
 * O modulo e puro C, sem dependencia em FreeRTOS ou ESP-IDF (exceto
 * esp_log opcional). Quem chama gamestate_tick() e quem decide quando
 * o tempo deve avancar (engine: so quando FSM == GAMEPLAY).
 */

void     gamestate_init(void);
void     gamestate_reset(void);

/* Avanca o tempo decorrido. Chame no tick do engine quando o jogo estiver
 * rodando (PAUSE deve nao chamar — assim o relogio congela). */
void     gamestate_tick(uint32_t dt_ms);

/* Hora atual do expediente em minutos desde meia-noite. Comeca em
 * HORA_INICIO_JOGO_MIN (08:00 = 480) e cresce ate HORA_FIM_JOGO_MIN
 * (18:00 = 1080), travando ali. */
uint16_t gamestate_get_clock_minutes(void);

/* Vidas do jogador. Inicia em GS_VIDAS_INICIAIS, perder_vida decrementa
 * com floor em 0 (sem underflow). Os gatilhos de quando perder uma vida
 * vivem fora deste modulo — quem decide e a logica de ameacas/cartas
 * (a ser implementada). */
uint8_t  gamestate_get_vidas(void);
void     gamestate_perder_vida(void);

/* Resultado da run. EM_ANDAMENTO ate o expediente terminar (vitoria) ou as
 * vidas acabarem (derrota). A tela final (GAME_OVER) le isto pra mostrar
 * "Promovido" (vitoria) vs "Demissao" (derrota). */
typedef enum {
    RESULT_EM_ANDAMENTO = 0,
    RESULT_VITORIA,
    RESULT_DERROTA,
} game_result_t;

void          gamestate_set_result(game_result_t r);
game_result_t gamestate_get_result(void);

#ifdef __cplusplus
}
#endif
