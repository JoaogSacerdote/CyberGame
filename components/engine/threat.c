#include "threat.h"

#include "game_config.h"
#include "gamestate.h"
#include "esp_log.h"

static const char *TAG = "THREAT";

static threat_state_t s_atk[THREAT_SERVER_COUNT];
static bool           s_ddos_spawnou[THREAT_SERVER_COUNT];
static bool           s_ransom_spawnou[THREAT_SERVER_COUNT];
/* Ransomware isolado com >=50% de dano: timer congelado aguardando Backup. */
static bool           s_congelado[THREAT_SERVER_COUNT];

void threat_init(void)
{
    for (uint8_t i = 0; i < THREAT_SERVER_COUNT; i++) {
        s_atk[i].ativo       = false;
        s_atk[i].tipo        = ATAQUE_DDOS;
        s_atk[i].restante_ms = 0;
        s_atk[i].total_ms    = 0;
        s_ddos_spawnou[i]    = false;
        s_ransom_spawnou[i]  = false;
        s_congelado[i]       = false;
    }
}

carta_id_t threat_carta_correta(ataque_tipo_t ataque)
{
    for (int c = 0; c < CARTA_MAX_COUNT; ++c) {
        if (defense_resolve((carta_id_t)c, ataque) == DEFESA_CORRETO) {
            return (carta_id_t)c;
        }
    }
    return CARTA_ISOLAMENTO;
}

static void spawn(uint8_t srv, ataque_tipo_t tipo)
{
    const uint32_t dur = (VERMELHO_TIMER_MS * 13u) / 10u;
    s_atk[srv].ativo       = true;
    s_atk[srv].tipo        = tipo;
    s_atk[srv].total_ms    = dur;
    s_atk[srv].restante_ms = dur;

    ESP_LOGI(TAG, "srv%u: %s ATIVO (mitiga: %s) — %u ms",
             (unsigned)srv,
             ataque_nome(tipo),
             carta_nome(threat_carta_correta(tipo)),
             (unsigned)dur);
}

static void try_spawn_scripted(uint8_t srv)
{
    if (!s_ddos_spawnou[srv] && gamestate_ddos_pode_spawnar(srv)) {
        spawn(srv, ATAQUE_DDOS);
        s_ddos_spawnou[srv] = true;
        return;
    }
    if (!s_ransom_spawnou[srv] && gamestate_ransomware_pode_spawnar(srv)) {
        spawn(srv, ATAQUE_RANSOMWARE);
        s_ransom_spawnou[srv] = true;
    }
}

bool threat_tick(uint8_t srv, uint32_t dt_ms)
{
    if (srv >= THREAT_SERVER_COUNT) return false;

    if (s_atk[srv].ativo) {
        /* Congelado (ransomware isolado >=50%): timer parado, nao expira.
         * O jogador pode sair, trocar os HDs (amarela) e voltar com o Backup. */
        if (s_congelado[srv]) {
            return false;
        }
        if (dt_ms >= s_atk[srv].restante_ms) {
            ESP_LOGW(TAG, "srv%u: %s NAO mitigado -> setor destruido",
                     (unsigned)srv, ataque_nome(s_atk[srv].tipo));
            s_atk[srv].ativo = false;
            return true;
        }
        s_atk[srv].restante_ms -= dt_ms;
        return false;
    }

    try_spawn_scripted(srv);
    return false;
}

defesa_resultado_t threat_mitigate(uint8_t srv, carta_id_t carta)
{
    if (srv >= THREAT_SERVER_COUNT || !s_atk[srv].ativo) {
        return DEFESA_INUTIL;
    }

    /* === Regra dinamica do RANSOMWARE (usuario, 2026-06-10) ===
     * Isolamento <50% de dano: servidor se recupera sozinho (mitigado).
     * Isolamento >=50%: dano grande demais — CONGELA o timer e passa a
     * exigir Backup. Backup so funciona congelado E com a tarefa amarela
     * do servidor concluida (todos os HDs bons). */
    if (s_atk[srv].tipo == ATAQUE_RANSOMWARE) {
        if (carta == CARTA_ISOLAMENTO) {
            if (threat_progress_pct(srv) < 50) {
                ESP_LOGI(TAG, "srv%u: Ransomware <50%% isolado -> recuperado sozinho",
                         (unsigned)srv);
                s_atk[srv].ativo = false;
                s_congelado[srv] = false;
                return DEFESA_CORRETO;
            }
            s_congelado[srv] = true;
            ESP_LOGW(TAG, "srv%u: Ransomware >=50%% isolado -> CONGELADO, "
                          "aguardando Backup de Emergencia", (unsigned)srv);
            return DEFESA_CONTIDO;
        }
        if (carta == CARTA_BACKUP) {
            if (!s_congelado[srv]) {
                ESP_LOGW(TAG, "srv%u: Backup sem isolar antes — malware ainda ativo, inutil",
                         (unsigned)srv);
                return DEFESA_INUTIL;
            }
            if (gamestate_amarela_estado(srv) != TAREFA_CONCLUIDA) {
                ESP_LOGW(TAG, "srv%u: Backup falhou — HDs danificados "
                              "(tarefa amarela pendente)", (unsigned)srv);
                return DEFESA_INUTIL;
            }
            ESP_LOGI(TAG, "srv%u: Backup com HDs bons -> sistema restaurado",
                     (unsigned)srv);
            s_atk[srv].ativo = false;
            s_congelado[srv] = false;
            return DEFESA_CORRETO;
        }
        /* Balanceamento: cai na matriz (AGRAVA). */
    }

    const defesa_resultado_t r = defense_resolve(carta, s_atk[srv].tipo);
    switch (r) {
        case DEFESA_CORRETO:
            ESP_LOGI(TAG, "srv%u: %s mitigado com %s",
                     (unsigned)srv, ataque_nome(s_atk[srv].tipo), carta_nome(carta));
            s_atk[srv].ativo = false;
            break;
        case DEFESA_AGRAVA: {
            const uint32_t corte =
                (s_atk[srv].restante_ms * VERMELHO_AGRAVADO_MULT_PCT) / 100;
            s_atk[srv].restante_ms =
                (corte < s_atk[srv].restante_ms) ? (s_atk[srv].restante_ms - corte) : 0;
            ESP_LOGW(TAG, "srv%u: %s AGRAVADO — restante %u ms",
                     (unsigned)srv, ataque_nome(s_atk[srv].tipo),
                     (unsigned)s_atk[srv].restante_ms);
            break;
        }
        case DEFESA_INUTIL:
        default:
            ESP_LOGW(TAG, "srv%u: %s: carta %s inutil",
                     (unsigned)srv, ataque_nome(s_atk[srv].tipo), carta_nome(carta));
            break;
    }
    return r;
}

bool threat_is_congelado(uint8_t srv)
{
    return srv < THREAT_SERVER_COUNT && s_atk[srv].ativo && s_congelado[srv];
}

bool threat_get_active(uint8_t srv, threat_state_t *out)
{
    if (srv >= THREAT_SERVER_COUNT) {
        if (out) { out->ativo = false; }
        return false;
    }
    if (out != NULL) {
        *out = s_atk[srv];
    }
    return s_atk[srv].ativo;
}

uint8_t threat_progress_pct(uint8_t srv)
{
    if (srv >= THREAT_SERVER_COUNT) return 0;
    if (!s_atk[srv].ativo || s_atk[srv].total_ms == 0) {
        return 0;
    }
    const uint32_t passed = s_atk[srv].total_ms - s_atk[srv].restante_ms;
    return (uint8_t)((passed * 100) / s_atk[srv].total_ms);
}
