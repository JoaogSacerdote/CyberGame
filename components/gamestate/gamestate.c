#include "gamestate.h"
#include "game_config.h"

static uint32_t s_elapsed_ms = 0;
static uint8_t  s_vidas      = VIDAS_INICIAIS;

void gamestate_init(void)
{
    s_elapsed_ms = 0;
    s_vidas      = VIDAS_INICIAIS;
}

void gamestate_reset(void)
{
    s_elapsed_ms = 0;
    s_vidas      = VIDAS_INICIAIS;
}

void gamestate_tick(uint32_t dt_ms)
{
    s_elapsed_ms += dt_ms;
    if (s_elapsed_ms > EXPEDIENTE_DURACAO_MS) {
        s_elapsed_ms = EXPEDIENTE_DURACAO_MS;
    }
}

uint16_t gamestate_get_clock_minutes(void)
{
    const uint32_t span     = (uint32_t)(HORA_FIM_JOGO_MIN - HORA_INICIO_JOGO_MIN);
    /* Multiplica antes de dividir pra preservar precisao; cabe em uint32
     * (180000 * 600 = 1.08e8 << 2^32). */
    const uint32_t advanced = (s_elapsed_ms * span) / EXPEDIENTE_DURACAO_MS;
    uint32_t mins = (uint32_t)HORA_INICIO_JOGO_MIN + advanced;
    if (mins > HORA_FIM_JOGO_MIN) {
        mins = HORA_FIM_JOGO_MIN;
    }
    return (uint16_t)mins;
}

uint8_t gamestate_get_vidas(void)
{
    return s_vidas;
}

void gamestate_perder_vida(void)
{
    if (s_vidas > 0) {
        s_vidas--;
    }
}
