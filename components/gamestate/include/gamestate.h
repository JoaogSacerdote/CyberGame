#pragma once

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void     gamestate_init(void);
void     gamestate_reset(void);

/* Inicia o expediente (chamado ao entrar no escritorio pela 1a vez).
 * Idempotente — chamadas subsequentes sao no-op. */
void     gamestate_iniciar_expediente(void);
bool     gamestate_expediente_ativo(void);

/* Avanca o tempo. PAUSE nao deve chamar — relogio congela.
 * Internamente sorteia e dispara spawns de tarefas. */
void     gamestate_tick(uint32_t dt_ms);

/* Estado das tarefas verde e amarela durante a run. */
typedef enum {
    TAREFA_AGUARDANDO = 0,   /* ainda nao disponivel */
    TAREFA_DISPONIVEL,        /* spawnou, aguarda jogador */
    TAREFA_CONCLUIDA,         /* concluida; estado salvo */
} tarefa_estado_t;

tarefa_estado_t  gamestate_verde_estado(void);
tarefa_estado_t  gamestate_amarela_estado(void);

/* Marca tarefa como concluida. Chamado ao confirmar na tela de tarefa. */
void             gamestate_concluir_verde(void);
void             gamestate_concluir_amarela(void);

/* Salva a selecao feita na tarefa verde (ponteiros para literais estaticos). */
void             gamestate_salvar_verde_selecao(const char *usuario, const char *senha);
void             gamestate_verde_selecao_get(const char **usuario, const char **senha);

/* Retorna true quando o tempo de expediente atingiu TAREFA_VERMELHO_MIN_MS,
 * liberando o spawn de ataques vermelhos. */
bool             gamestate_vermelho_pode_spawnar(void);

/* Compat: equivalem a estado >= DISPONIVEL. */
static inline bool gamestate_verde_spawned(void)   { return gamestate_verde_estado()   >= TAREFA_DISPONIVEL; }
static inline bool gamestate_amarela_spawned(void) { return gamestate_amarela_estado() >= TAREFA_DISPONIVEL; }

/* Hora atual em minutos desde meia-noite (08:00=480 ate 18:00=1080). */
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
