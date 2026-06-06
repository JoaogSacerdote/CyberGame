#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "nfc_config.h"   /* carta_id_t + NFC_CARTAS */

#ifdef __cplusplus
extern "C" {
#endif

/* Trio de ataques do MVP (RESPOSTAS.txt / game_logic_decisions). */
typedef enum {
    ATAQUE_DDOS = 0,
    ATAQUE_RANSOMWARE,
    ATAQUE_PROPAGACAO_LATERAL,
    ATAQUE_MAX_COUNT,
} ataque_tipo_t;

/* Resultado de aplicar uma carta de defesa contra um ataque ativo. */
typedef enum {
    DEFESA_CORRETO = 0,   /* mitiga o ataque                          */
    DEFESA_INUTIL,        /* sem efeito — so perde tempo              */
    DEFESA_AGRAVA,        /* piora — acelera o timer do ataque        */
} defesa_resultado_t;

/* Matriz carta x ataque. Fonte: game_logic_decisions.
 *
 *               | DDoS   | Ransomware | Propagacao
 *  Isolamento   | inutil | inutil     | CORRETO
 *  Backup       | AGRAVA | CORRETO    | inutil
 *  Balanceamento| CORRETO| AGRAVA     | inutil
 */
defesa_resultado_t defense_resolve(carta_id_t carta, ataque_tipo_t ataque);

/* UID fisico lido pelo PN532 -> carta (tabela NFC_CARTAS de nfc_config.h).
 * true se reconhecido. NOTA: enquanto os UIDs forem placeholder (0xDEADBEEF),
 * so reconhece se as tags reais coincidirem — preencher nfc_config.h. */
bool nfc_uid_to_carta(const uint8_t *uid, uint8_t uid_len, carta_id_t *out);

/* Nomes para log / UI. */
const char *carta_nome(carta_id_t c);
const char *ataque_nome(ataque_tipo_t a);
const char *defesa_resultado_nome(defesa_resultado_t r);

/* Imprime a matriz inteira no log — validacao remota pelo boot.
 * (Aux temporario; pode sair quando o gameplay estiver validado.) */
void defense_matrix_selftest(void);

#ifdef __cplusplus
}
#endif
