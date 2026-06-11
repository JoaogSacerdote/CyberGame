#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "defense_matrix.h"   /* ataque_tipo_t, carta_id_t, defesa_resultado_t */

#ifdef __cplusplus
extern "C" {
#endif

/* Estado do ataque vermelho ativo. */
typedef struct {
    bool          ativo;
    ataque_tipo_t tipo;
    uint32_t      restante_ms;   /* tempo ate destruir o setor */
    uint32_t      total_ms;      /* duracao base (pra calcular o %)*/
} threat_state_t;

/* Numero de servidores independentes (espelha WEB_SETOR_COUNT). */
#define THREAT_SERVER_COUNT  2

/* Zera o sistema: nenhum ataque, timers de spawn rearmados. Chamar no inicio
 * de cada run (entrada fresca em GAMEPLAY). */
void threat_init(void);

/* Avanca o servidor srv (0 ou 1) em dt_ms. Faz spawn no intervalo e conta o
 * timer do ataque ativo. Retorna true se um ataque EXPIROU neste tick (setor
 * destruido) — o caller deve perder uma vida. */
bool threat_tick(uint8_t srv, uint32_t dt_ms);

/* Aplica uma carta contra o ataque ativo do servidor srv.
 * CORRETO limpa o ataque; AGRAVA acelera o timer; INUTIL nao faz nada.
 *
 * RANSOMWARE tem regra dinamica (regra do usuario, 2026-06-10):
 *  - ISOLAMENTO com progresso <50%  -> CORRETO (servidor se recupera sozinho)
 *  - ISOLAMENTO com progresso >=50% -> CONTIDO (timer CONGELA; falta Backup)
 *  - BACKUP so funciona com o ataque congelado E a tarefa amarela daquele
 *    servidor concluida (todos os HDs bons); senao INUTIL. */
defesa_resultado_t threat_mitigate(uint8_t srv, carta_id_t carta);

/* true se o ransomware do servidor srv esta congelado aguardando Backup. */
bool threat_is_congelado(uint8_t srv);

/* Copia o estado do ataque ativo do servidor srv pra *out.
 * Retorna true se ha ataque ativo. */
bool threat_get_active(uint8_t srv, threat_state_t *out);

/* 0..100 — quao perto de destruir o setor srv (100 = vai destruir agora). */
uint8_t threat_progress_pct(uint8_t srv);

/* A carta que MITIGA um dado ataque (a coluna CORRETO da matriz). */
carta_id_t threat_carta_correta(ataque_tipo_t ataque);

#ifdef __cplusplus
}
#endif
