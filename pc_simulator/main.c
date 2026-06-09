/* ============================================================================
 * CyberSim — Simulador PC do firmware CyberGame
 *
 * Janela SDL 480x320, LVGL 9, sem FreeRTOS. O engine roda inteiro dentro de
 * lv_timer callbacks (ver sim_engine.c). Teclado mapeia botoes do hardware:
 *
 *   Setas       — direcao do joystick
 *   Z           — BTN_A
 *   X           — BTN_B
 *   C           — BTN_X
 *   V           — BTN_Y
 *   Enter       — BTN_START
 * ============================================================================ */

#include <SDL2/SDL.h>
#include <stdio.h>

#include "lvgl.h"
#include "src/drivers/sdl/lv_sdl_window.h"
#include "engine.h"
#include "esp_err.h"

#define SIM_DISPLAY_W   480
#define SIM_DISPLAY_H   320

int main(int argc, char **argv)
{
    (void)argc; (void)argv;

    lv_init();

    lv_display_t *disp = lv_sdl_window_create(SIM_DISPLAY_W, SIM_DISPLAY_H);
    if (disp == NULL) {
        fprintf(stderr, "Falha ao criar janela SDL\n");
        return 1;
    }
    lv_sdl_window_set_title(disp,
        "CyberSim  |  Setas=mover  Space=A  Z=A  X=B  V=Y(interagir)  Enter=START");

    lv_sdl_mouse_create();
    lv_sdl_keyboard_create();

    if (engine_init() != ESP_OK) {
        fprintf(stderr, "engine_init falhou\n");
        return 1;
    }
    if (engine_start() != ESP_OK) {
        fprintf(stderr, "engine_start falhou\n");
        return 1;
    }

    while (1) {
        /* Pump OS events into SDL queue ANTES do lv_timer_handler, garantindo
         * que SDL_GetKeyboardState() esteja atualizado quando os timers do
         * engine/splash chamarem button_hal_peek(). */
        SDL_PumpEvents();

        uint32_t delay = lv_timer_handler();
        if (delay == LV_NO_TIMER_READY) delay = 5;
        if (delay > 100) delay = 100;
        SDL_Delay(delay);
    }
    return 0;
}
