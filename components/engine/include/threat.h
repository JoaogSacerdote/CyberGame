#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "defense_matrix.h"   /* ataque_tipo_t, carta_id_t, defesa_resultado_t */

#ifdef __cplusplus
extern "C" {
#endif

/* Estado do ataque vermelho ativo. So 1 por vez (MAX_VERMELHOS_SIMULTANEOS). */
typedef struct {
    bool          ativo;
    ataque_tipo_t tipo;
    uint32_t      restante_ms;   /* tempo ate destruir o setor */
    uint32_t      total_ms;      /* duracao base (pra calcular o %)*/
} threat_state_t;

/* Zera o sistema: nenhum ataque, timer de spawn rearmado. Chamar no inicio
 * de cada run (entrada fresca em GAMEPLAY). */
void threat_init(void);

/* Avanca o sistema em dt_ms. Faz spawn no intervalo e conta o timer do
 * ataque ativo. Retorna true se um ataque EXPIROU neste tick (setor
 * destruido) — o caller deve perder uma vida. */
bool threat_tick(uint32_t dt_ms);

/* Aplica uma carta contra o ataque ativo (matriz). CORRETO limpa o ataque;
 * AGRAVA acelera o timer; INUTIL nao faz nada. Sem ataque ativo -> INUTIL. */
defesa_resultado_t threat_mitigate(carta_id_t carta);

/* Copia o estado do ataque ativo pra *out. true se ha ataque ativo. */
bool threat_get_active(threat_state_t *out);

/* 0..100 — quao perto de destruir o setor (100 = vai destruir agora). */
uint8_t threat_progress_pct(void);

/* A carta que MITIGA um dado ataque (a coluna CORRETO da matriz). Util pra
 * mocks/sim e pra dicas de UI. */
carta_id_t threat_carta_correta(ataque_tipo_t ataque);

#ifdef __cplusplus
}
#endif
