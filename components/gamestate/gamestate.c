#include "gamestate.h"
#include "game_config.h"

#include "esp_random.h"
#include "esp_log.h"

static const char *TAG = "GAMESTATE";

static uint32_t        s_elapsed_ms       = 0;
static uint8_t         s_vidas            = VIDAS_INICIAIS;
static game_result_t   s_result           = RESULT_EM_ANDAMENTO;
static bool            s_expediente_ativo = false;

/* Estado das tarefas */
static tarefa_estado_t s_verde_estado      = TAREFA_AGUARDANDO;
static tarefa_estado_t s_amarela_estado[2] = { TAREFA_AGUARDANDO, TAREFA_AGUARDANDO };

/* Seleção salva da tarefa verde */
static const char *s_verde_usuario = NULL;
static const char *s_verde_senha   = NULL;

/* ── Sequência scripted ─────────────────────────────────────────────────────
 *
 * Ordem dos eventos por run:
 *   1. VERDE          (senha)           — t_verde_spawn
 *   2. AMARELA #1     (srv_primeiro)    — t_am1_spawn
 *   3. DDoS           (srv_segundo)     — t_ddos_spawn  [gate para threat.c]
 *   4. AMARELA #2     (srv_segundo)     — t_am2_spawn
 *   5. RANSOMWARE     (srv_primeiro)    — t_ransom_spawn [gate para threat.c]
 *                                         calculado para o timer expirar
 *                                         RANSOM_END_BEFORE_MS antes do fim
 *                                         do expediente.
 *
 * srv_primeiro e srv_segundo são sorteados no reset (0 ou 1).
 * ─────────────────────────────────────────────────────────────────────────── */
static uint8_t  s_srv_primeiro   = 0;
static uint8_t  s_srv_segundo    = 1;

static uint32_t s_t_verde_spawn  = 0;
static uint32_t s_t_am1_spawn    = 0;
static uint32_t s_t_ddos_spawn   = 0;
static uint32_t s_t_am2_spawn    = 0;
static uint32_t s_t_ransom_spawn = 0;

static void reset_all(void)
{
    s_elapsed_ms       = 0;
    s_vidas            = VIDAS_INICIAIS;
    s_result           = RESULT_EM_ANDAMENTO;
    s_expediente_ativo = false;
    s_verde_estado        = TAREFA_AGUARDANDO;
    s_amarela_estado[0]   = TAREFA_AGUARDANDO;
    s_amarela_estado[1]   = TAREFA_AGUARDANDO;
    s_verde_usuario    = NULL;
    s_verde_senha      = NULL;

    /* Sorteia qual servidor recebe amarela #1 e ransomware. */
    s_srv_primeiro = (uint8_t)(esp_random() & 1u);
    s_srv_segundo  = 1u - s_srv_primeiro;

    /* Distribuição (usuário, 2026-06-11): verde fixa em VERDE_SPAWN_MS (5 s);
     * ransomware ancorado para TERMINAR (timer expirar) RANSOM_END_BEFORE_MS
     * (20 s) antes do fim do expediente; os 3 eventos do meio dividem o
     * intervalo [verde, ransom] em 4 passos IGUAIS.
     *
     * Verde ──step── Am1 ──step── DDoS ──step── Am2 ──step── Ransom
     *
     * Fallback: se o expediente for curto demais para a âncora (calibração
     * de relógio muito rápida), espaça pelo piso TAREFA_SPAWN_GAP_MS e o
     * ransomware pode atravessar as 18:00 — a vitória no fim continua valendo. */
    s_t_verde_spawn = VERDE_SPAWN_MS;

    uint32_t ransom_spawn = s_t_verde_spawn + 4u * TAREFA_SPAWN_GAP_MS;
    if (EXPEDIENTE_DURACAO_MS > RANSOM_END_BEFORE_MS + VERMELHO_TIMER_MS) {
        const uint32_t alvo =
            EXPEDIENTE_DURACAO_MS - RANSOM_END_BEFORE_MS - VERMELHO_TIMER_MS;
        if (alvo > ransom_spawn) ransom_spawn = alvo;
    }

    const uint32_t step = (ransom_spawn - s_t_verde_spawn) / 4u;
    s_t_am1_spawn    = s_t_verde_spawn + step;
    s_t_ddos_spawn   = s_t_verde_spawn + 2u * step;
    s_t_am2_spawn    = s_t_verde_spawn + 3u * step;
    s_t_ransom_spawn = ransom_spawn;

    ESP_LOGI(TAG,
        "seq: srv_pri=%u srv_seg=%u | verde=%u am1=%u ddos=%u am2=%u ransom=%u (ms)",
        (unsigned)s_srv_primeiro, (unsigned)s_srv_segundo,
        (unsigned)s_t_verde_spawn, (unsigned)s_t_am1_spawn,
        (unsigned)s_t_ddos_spawn,  (unsigned)s_t_am2_spawn,
        (unsigned)s_t_ransom_spawn);
}

void gamestate_init(void)  { reset_all(); }
void gamestate_reset(void) { reset_all(); }

void gamestate_iniciar_expediente(void)
{
    s_expediente_ativo = true;
}

bool gamestate_expediente_ativo(void)
{
    return s_expediente_ativo;
}

void gamestate_tick(uint32_t dt_ms)
{
    if (!s_expediente_ativo) return;

    s_elapsed_ms += dt_ms;
    if (s_elapsed_ms > EXPEDIENTE_DURACAO_MS) {
        s_elapsed_ms = EXPEDIENTE_DURACAO_MS;
    }

    /* === Tarefa VERDE === */
    if (s_verde_estado == TAREFA_AGUARDANDO && s_elapsed_ms >= s_t_verde_spawn) {
        s_verde_estado = TAREFA_DISPONIVEL;
        ESP_LOGI(TAG, "VERDE disponivel (t=%u ms)", (unsigned)s_elapsed_ms);
    }

    /* === Tarefa AMARELA #1 (srv_primeiro) === */
    if (s_amarela_estado[s_srv_primeiro] == TAREFA_AGUARDANDO &&
        s_elapsed_ms >= s_t_am1_spawn) {
        s_amarela_estado[s_srv_primeiro] = TAREFA_DISPONIVEL;
        ESP_LOGI(TAG, "AMARELA #1 srv%u disponivel (t=%u ms)",
                 (unsigned)s_srv_primeiro, (unsigned)s_elapsed_ms);
    }

    /* === Tarefa AMARELA #2 (srv_segundo) === */
    if (s_amarela_estado[s_srv_segundo] == TAREFA_AGUARDANDO &&
        s_elapsed_ms >= s_t_am2_spawn) {
        s_amarela_estado[s_srv_segundo] = TAREFA_DISPONIVEL;
        ESP_LOGI(TAG, "AMARELA #2 srv%u disponivel (t=%u ms)",
                 (unsigned)s_srv_segundo, (unsigned)s_elapsed_ms);
    }
}

tarefa_estado_t gamestate_verde_estado(void) { return s_verde_estado; }

tarefa_estado_t gamestate_amarela_estado(uint8_t srv)
{
    return (srv < 2) ? s_amarela_estado[srv] : TAREFA_AGUARDANDO;
}

void gamestate_concluir_verde(void)
{
    s_verde_estado = TAREFA_CONCLUIDA;
    ESP_LOGI(TAG, "VERDE concluida");
}

void gamestate_concluir_amarela(uint8_t srv)
{
    if (srv < 2) {
        s_amarela_estado[srv] = TAREFA_CONCLUIDA;
        ESP_LOGI(TAG, "AMARELA srv%u concluida", (unsigned)srv);
    }
}

void gamestate_salvar_verde_selecao(const char *usuario, const char *senha)
{
    s_verde_usuario = usuario;
    s_verde_senha   = senha;
}

void gamestate_verde_selecao_get(const char **usuario, const char **senha)
{
    if (usuario) *usuario = s_verde_usuario;
    if (senha)   *senha   = s_verde_senha;
}

bool gamestate_ddos_pode_spawnar(uint8_t srv)
{
    return s_expediente_ativo
        && (srv == s_srv_segundo)
        && (s_elapsed_ms >= s_t_ddos_spawn);
}

bool gamestate_ransomware_pode_spawnar(uint8_t srv)
{
    return s_expediente_ativo
        && (srv == s_srv_primeiro)
        && (s_elapsed_ms >= s_t_ransom_spawn);
}

uint16_t gamestate_get_clock_minutes(void)
{
    const uint32_t span     = (uint32_t)(HORA_FIM_JOGO_MIN - HORA_INICIO_JOGO_MIN);
    const uint32_t advanced = (s_elapsed_ms * span) / EXPEDIENTE_DURACAO_MS;
    uint32_t mins = (uint32_t)HORA_INICIO_JOGO_MIN + advanced;
    if (mins > HORA_FIM_JOGO_MIN) mins = HORA_FIM_JOGO_MIN;
    return (uint16_t)mins;
}

uint8_t gamestate_get_vidas(void) { return s_vidas; }

void gamestate_perder_vida(void)
{
    if (s_vidas > 0) s_vidas--;
}

void gamestate_set_result(game_result_t r) { s_result = r; }
game_result_t gamestate_get_result(void)   { return s_result; }
