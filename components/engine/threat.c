#include "threat.h"

#include "game_config.h"
#include "esp_log.h"

static const char *TAG = "THREAT";

static threat_state_t s_atk;
static uint32_t       s_spawn_timer_ms;   /* contagem regressiva pro proximo spawn */
static uint8_t        s_next_tipo;         /* cicla os 3 ataques pra variedade */

void threat_init(void)
{
    s_atk.ativo       = false;
    s_atk.tipo        = ATAQUE_DDOS;
    s_atk.restante_ms = 0;
    s_atk.total_ms    = 0;
    s_spawn_timer_ms  = EVENTO_VERMELHO_PRIMEIRO_MS;   /* 1o ataque vem cedo */
    s_next_tipo       = 0;
}

carta_id_t threat_carta_correta(ataque_tipo_t ataque)
{
    for (int c = 0; c < CARTA_MAX_COUNT; ++c) {
        if (defense_resolve((carta_id_t)c, ataque) == DEFESA_CORRETO) {
            return (carta_id_t)c;
        }
    }
    return CARTA_ISOLAMENTO;   /* fallback — nao deveria acontecer */
}

static void spawn(void)
{
    s_atk.ativo       = true;
    s_atk.tipo        = (ataque_tipo_t)(s_next_tipo % ATAQUE_MAX_COUNT);
    s_next_tipo++;
    s_atk.total_ms    = VERMELHO_TIMER_MS;
    s_atk.restante_ms = VERMELHO_TIMER_MS;
    ESP_LOGI(TAG, "ataque ATIVO: %s (mitiga com %s) — %u ms",
             ataque_nome(s_atk.tipo),
             carta_nome(threat_carta_correta(s_atk.tipo)),
             (unsigned)s_atk.restante_ms);
}

bool threat_tick(uint32_t dt_ms)
{
    if (s_atk.ativo) {
        if (dt_ms >= s_atk.restante_ms) {
            ESP_LOGW(TAG, "ataque %s NAO mitigado -> setor destruido",
                     ataque_nome(s_atk.tipo));
            s_atk.ativo      = false;
            s_spawn_timer_ms = EVENTO_VERMELHO_INTERVALO_MS;
            return true;   /* setor destruido neste tick */
        }
        s_atk.restante_ms -= dt_ms;
        return false;
    }

    /* sem ataque ativo: conta pro proximo spawn */
    if (dt_ms >= s_spawn_timer_ms) {
        spawn();
    } else {
        s_spawn_timer_ms -= dt_ms;
    }
    return false;
}

defesa_resultado_t threat_mitigate(carta_id_t carta)
{
    if (!s_atk.ativo) {
        return DEFESA_INUTIL;
    }
    const defesa_resultado_t r = defense_resolve(carta, s_atk.tipo);
    switch (r) {
        case DEFESA_CORRETO:
            ESP_LOGI(TAG, "%s mitigado com %s",
                     ataque_nome(s_atk.tipo), carta_nome(carta));
            s_atk.ativo      = false;
            s_spawn_timer_ms = EVENTO_VERMELHO_INTERVALO_MS;
            break;
        case DEFESA_AGRAVA: {
            const uint32_t corte =
                (s_atk.restante_ms * VERMELHO_AGRAVADO_MULT_PCT) / 100;
            s_atk.restante_ms =
                (corte < s_atk.restante_ms) ? (s_atk.restante_ms - corte) : 0;
            ESP_LOGW(TAG, "%s AGRAVADO por %s — restante %u ms",
                     ataque_nome(s_atk.tipo), carta_nome(carta),
                     (unsigned)s_atk.restante_ms);
            break;
        }
        case DEFESA_INUTIL:
        default:
            ESP_LOGW(TAG, "%s: carta %s inutil (so perdeu tempo)",
                     ataque_nome(s_atk.tipo), carta_nome(carta));
            break;
    }
    return r;
}

bool threat_get_active(threat_state_t *out)
{
    if (out != NULL) {
        *out = s_atk;
    }
    return s_atk.ativo;
}

uint8_t threat_progress_pct(void)
{
    if (!s_atk.ativo || s_atk.total_ms == 0) {
        return 0;
    }
    const uint32_t passed = s_atk.total_ms - s_atk.restante_ms;
    return (uint8_t)((passed * 100) / s_atk.total_ms);
}
