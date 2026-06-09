#include "gamestate.h"
#include "game_config.h"

#include "esp_random.h"
#include "esp_log.h"

static const char *TAG = "GAMESTATE";

static uint32_t        s_elapsed_ms         = 0;
static uint8_t         s_vidas              = VIDAS_INICIAIS;
static game_result_t   s_result             = RESULT_EM_ANDAMENTO;
static bool            s_expediente_ativo   = false;

/* Estado das tarefas */
static tarefa_estado_t s_verde_estado       = TAREFA_AGUARDANDO;
static tarefa_estado_t s_amarela_estado     = TAREFA_AGUARDANDO;

/* Momento alvo (ms de expediente) em que cada tarefa spawna — decidido
 * aleatoriamente na primeira vez que o minimo e atingido. 0 = nao decidido. */
static uint32_t        s_verde_spawn_at     = 0;
static uint32_t        s_amarela_spawn_at   = 0;

/* Momento do ultimo spawn (para garantir gap entre spawns). */
static uint32_t        s_last_spawn_ms      = 0;

/* Selecao salva da tarefa verde (ponteiros para literais estaticos). */
static const char     *s_verde_usuario      = NULL;
static const char     *s_verde_senha        = NULL;

static void reset_all(void)
{
    s_elapsed_ms       = 0;
    s_vidas            = VIDAS_INICIAIS;
    s_result           = RESULT_EM_ANDAMENTO;
    s_expediente_ativo = false;
    s_verde_estado     = TAREFA_AGUARDANDO;
    s_amarela_estado   = TAREFA_AGUARDANDO;
    s_verde_spawn_at   = 0;
    s_amarela_spawn_at = 0;
    s_last_spawn_ms    = 0;
    s_verde_usuario    = NULL;
    s_verde_senha      = NULL;
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
    if (s_verde_estado == TAREFA_AGUARDANDO) {
        if (s_verde_spawn_at == 0 && s_elapsed_ms >= TAREFA_VERDE_MIN_MS) {
            uint32_t rnd = (TAREFA_VERDE_RAND_MS > 0)
                           ? (esp_random() % TAREFA_VERDE_RAND_MS) : 0;
            s_verde_spawn_at = s_elapsed_ms + rnd;
        }
        if (s_verde_spawn_at > 0 && s_elapsed_ms >= s_verde_spawn_at) {
            uint32_t pode = (s_last_spawn_ms > 0)
                            ? s_last_spawn_ms + TAREFA_SPAWN_GAP_MS : 0;
            if (s_elapsed_ms >= pode) {
                s_verde_estado  = TAREFA_DISPONIVEL;
                s_last_spawn_ms = s_elapsed_ms;
                ESP_LOGI(TAG, "tarefa VERDE disponivel (t=%u ms)", (unsigned)s_elapsed_ms);
            }
        }
    }

    /* === Tarefa AMARELA === */
    if (s_amarela_estado == TAREFA_AGUARDANDO) {
        if (s_amarela_spawn_at == 0 && s_elapsed_ms >= TAREFA_AMARELA_MIN_MS) {
            uint32_t rnd = (TAREFA_AMARELA_RAND_MS > 0)
                           ? (esp_random() % TAREFA_AMARELA_RAND_MS) : 0;
            s_amarela_spawn_at = s_elapsed_ms + rnd;
        }
        if (s_amarela_spawn_at > 0 && s_elapsed_ms >= s_amarela_spawn_at) {
            uint32_t pode = (s_last_spawn_ms > 0)
                            ? s_last_spawn_ms + TAREFA_SPAWN_GAP_MS : 0;
            if (s_elapsed_ms >= pode) {
                s_amarela_estado = TAREFA_DISPONIVEL;
                s_last_spawn_ms  = s_elapsed_ms;
                ESP_LOGI(TAG, "tarefa AMARELA disponivel (t=%u ms)", (unsigned)s_elapsed_ms);
            }
        }
    }
}

tarefa_estado_t gamestate_verde_estado(void)   { return s_verde_estado; }
tarefa_estado_t gamestate_amarela_estado(void) { return s_amarela_estado; }

void gamestate_concluir_verde(void)
{
    s_verde_estado = TAREFA_CONCLUIDA;
    ESP_LOGI(TAG, "tarefa VERDE marcada como concluida");
}

void gamestate_concluir_amarela(void)
{
    s_amarela_estado = TAREFA_CONCLUIDA;
    ESP_LOGI(TAG, "tarefa AMARELA marcada como concluida");
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

bool gamestate_vermelho_pode_spawnar(void)
{
    return s_expediente_ativo && s_elapsed_ms >= TAREFA_VERMELHO_MIN_MS;
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
