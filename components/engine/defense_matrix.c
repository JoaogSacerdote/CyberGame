#include "defense_matrix.h"

#include <string.h>
#include "esp_log.h"

static const char *TAG = "DEFESA";

/* Linhas = carta (CARTA_ISOLAMENTO/BACKUP/BALANCEAMENTO).
 * Colunas = ataque (ATAQUE_DDOS/RANSOMWARE/PROPAGACAO_LATERAL). */
static const defesa_resultado_t MATRIZ[CARTA_MAX_COUNT][ATAQUE_MAX_COUNT] = {
    [CARTA_ISOLAMENTO]    = { DEFESA_INUTIL,  DEFESA_INUTIL,  DEFESA_CORRETO },
    [CARTA_BACKUP]        = { DEFESA_AGRAVA,  DEFESA_CORRETO, DEFESA_INUTIL  },
    [CARTA_BALANCEAMENTO] = { DEFESA_CORRETO, DEFESA_AGRAVA,  DEFESA_INUTIL  },
};

defesa_resultado_t defense_resolve(carta_id_t carta, ataque_tipo_t ataque)
{
    if (carta >= CARTA_MAX_COUNT || ataque >= ATAQUE_MAX_COUNT) {
        return DEFESA_INUTIL;   /* fora da faixa: trata como inofensivo */
    }
    return MATRIZ[carta][ataque];
}

bool nfc_uid_to_carta(const uint8_t *uid, uint8_t uid_len, carta_id_t *out)
{
    if (uid == NULL || out == NULL || uid_len == 0) {
        return false;
    }
    for (int i = 0; i < CARTA_MAX_COUNT; ++i) {
        const nfc_card_entry_t *e = &NFC_CARTAS[i];
        if (e->uid_len == uid_len && memcmp(e->uid, uid, uid_len) == 0) {
            *out = e->carta;
            return true;
        }
    }
    return false;
}

const char *carta_nome(carta_id_t c)
{
    switch (c) {
        case CARTA_ISOLAMENTO:    return "Isolamento";
        case CARTA_BACKUP:        return "Backup";
        case CARTA_BALANCEAMENTO: return "Balanceamento";
        default:                  return "?";
    }
}

const char *ataque_nome(ataque_tipo_t a)
{
    switch (a) {
        case ATAQUE_DDOS:               return "DDoS";
        case ATAQUE_RANSOMWARE:         return "Ransomware";
        case ATAQUE_PROPAGACAO_LATERAL: return "Propagacao Lateral";
        default:                        return "?";
    }
}

const char *defesa_resultado_nome(defesa_resultado_t r)
{
    switch (r) {
        case DEFESA_CORRETO: return "CORRETO";
        case DEFESA_INUTIL:  return "inutil";
        case DEFESA_AGRAVA:  return "AGRAVA";
        default:             return "?";
    }
}

void defense_matrix_selftest(void)
{
    ESP_LOGI(TAG, "=== Matriz carta x ataque ===");
    for (int ci = 0; ci < CARTA_MAX_COUNT; ++ci) {
        const carta_id_t c = (carta_id_t)ci;
        ESP_LOGI(TAG, "  %-14s | DDoS=%-7s Ransom=%-7s Propag=%-7s",
                 carta_nome(c),
                 defesa_resultado_nome(defense_resolve(c, ATAQUE_DDOS)),
                 defesa_resultado_nome(defense_resolve(c, ATAQUE_RANSOMWARE)),
                 defesa_resultado_nome(defense_resolve(c, ATAQUE_PROPAGACAO_LATERAL)));
    }
}
