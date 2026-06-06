#include "gamestate.h"
#include "test_runner.h"

/* (a) Apos init, relogio comeca exatamente em 08:00 (480 minutos). */
static void test_init_starts_at_8h(void) {
    gamestate_init();
    TEST_EQ_INT(gamestate_get_clock_minutes(), 480);
}

/* (b) Tick parcial: 30s reais = 1/6 do expediente = 100 minutos in-game.
 *     480 + 100 = 580 = 09:40. */
static void test_partial_tick_advances_proportional(void) {
    gamestate_init();
    gamestate_tick(30 * 1000);
    TEST_EQ_INT(gamestate_get_clock_minutes(), 580);
}

/* (c) Tick exato do expediente inteiro = 18:00 (1080). */
static void test_full_expediente_reaches_18h(void) {
    gamestate_init();
    gamestate_tick(3 * 60 * 1000); /* 180s */
    TEST_EQ_INT(gamestate_get_clock_minutes(), 1080);
}

/* (d) Tick alem do limite trava em 18:00 (overflow protegido). */
static void test_tick_beyond_clamps(void) {
    gamestate_init();
    gamestate_tick(10 * 60 * 1000); /* 10 min reais, bem alem do expediente */
    TEST_EQ_INT(gamestate_get_clock_minutes(), 1080);
}

/* (e) Reset volta pra 08:00 mesmo apos avancar. */
static void test_reset_returns_to_8h(void) {
    gamestate_init();
    gamestate_tick(60 * 1000); /* avanca */
    gamestate_reset();
    TEST_EQ_INT(gamestate_get_clock_minutes(), 480);
}

/* (f) Ticks incrementais acumulam corretamente (anti-regressao). */
static void test_incremental_ticks_accumulate(void) {
    gamestate_init();
    for (int i = 0; i < 100; ++i) {
        gamestate_tick(100); /* 100 x 100ms = 10s */
    }
    /* 10s = 1/18 do expediente = ~33.3 minutos in-game.
     * 480 + (10000 * 600 / 180000) = 480 + 33 = 513. */
    TEST_EQ_INT(gamestate_get_clock_minutes(), 513);
}

/* (g) Vidas iniciam em 3 apos init. */
static void test_vidas_inicia_em_3(void) {
    gamestate_init();
    TEST_EQ_INT(gamestate_get_vidas(), 3);
}

/* (h) perder_vida decrementa em 1. */
static void test_perder_vida_decrementa(void) {
    gamestate_init();
    gamestate_perder_vida();
    TEST_EQ_INT(gamestate_get_vidas(), 2);
    gamestate_perder_vida();
    TEST_EQ_INT(gamestate_get_vidas(), 1);
}

/* (i) perder_vida alem de 0 trava em 0 (sem underflow de uint8). */
static void test_perder_vida_floor_zero(void) {
    gamestate_init();
    for (int i = 0; i < 10; ++i) gamestate_perder_vida();
    TEST_EQ_INT(gamestate_get_vidas(), 0);
}

/* (j) Reset restaura pra 3 mesmo apos ter perdido todas. */
static void test_reset_restaura_vidas(void) {
    gamestate_init();
    for (int i = 0; i < 5; ++i) gamestate_perder_vida();
    TEST_EQ_INT(gamestate_get_vidas(), 0);
    gamestate_reset();
    TEST_EQ_INT(gamestate_get_vidas(), 3);
}

int main(void) {
    TEST_RUN(test_init_starts_at_8h);
    TEST_RUN(test_partial_tick_advances_proportional);
    TEST_RUN(test_full_expediente_reaches_18h);
    TEST_RUN(test_tick_beyond_clamps);
    TEST_RUN(test_reset_returns_to_8h);
    TEST_RUN(test_incremental_ticks_accumulate);
    TEST_RUN(test_vidas_inicia_em_3);
    TEST_RUN(test_perder_vida_decrementa);
    TEST_RUN(test_perder_vida_floor_zero);
    TEST_RUN(test_reset_restaura_vidas);
    TEST_SUMMARY();
}
