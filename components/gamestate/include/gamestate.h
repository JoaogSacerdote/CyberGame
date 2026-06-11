#pragma once

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void     gamestate_init(void);
void     gamestate_reset(void);

/* Inicia o expediente (chamado ao entrar no escritório pela 1ª vez). Idempotente. */
void     gamestate_iniciar_expediente(void);
bool     gamestate_expediente_ativo(void);

/* Avança o tempo. PAUSE não deve chamar — relógio congela. */
void     gamestate_tick(uint32_t dt_ms);

/* Estado das tarefas verde e amarela durante a run. */
typedef enum {
    TAREFA_AGUARDANDO = 0,   /* ainda não disponível */
    TAREFA_DISPONIVEL,        /* spawnou, aguarda jogador */
    TAREFA_CONCLUIDA,         /* concluída; estado salvo */
} tarefa_estado_t;

tarefa_estado_t  gamestate_verde_estado(void);
/* srv = 0 (servidor esquerda) ou 1 (servidor direita). */
tarefa_estado_t  gamestate_amarela_estado(uint8_t srv);

/* Marca tarefa como concluída. Chamado ao confirmar na tela de tarefa. */
void             gamestate_concluir_verde(void);
void             gamestate_concluir_amarela(uint8_t srv);

/* Salva a seleção feita na tarefa verde (ponteiros para literais estáticos). */
void             gamestate_salvar_verde_selecao(const char *usuario, const char *senha);
void             gamestate_verde_selecao_get(const char **usuario, const char **senha);

/* Gates de ataque para threat.c.
 * Retorna true quando o ataque pode spawnar naquele servidor segundo a sequência. */
bool             gamestate_ddos_pode_spawnar(uint8_t srv);
bool             gamestate_ransomware_pode_spawnar(uint8_t srv);

/* Compat: equivalem a estado >= DISPONIVEL. */
static inline bool gamestate_verde_spawned(void)          { return gamestate_verde_estado()      >= TAREFA_DISPONIVEL; }
static inline bool gamestate_amarela_spawned(uint8_t srv) { return gamestate_amarela_estado(srv) >= TAREFA_DISPONIVEL; }

/* Hora atual em minutos desde meia-noite (08:00=480 até 18:00=1080). */
uint16_t gamestate_get_clock_minutes(void);

/* Vidas. */
uint8_t  gamestate_get_vidas(void);
void     gamestate_perder_vida(void);

/* Resultado da run. */
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
