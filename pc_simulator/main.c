/* ============================================================================
 * CyberSim — Simulador PC do firmware CyberGame
 *
 * Etapa 1: prova de conceito. Abre janela SDL 480x320 (mesma resolucao do
 * display do aparelho) com LVGL renderizando dentro. Tela vazia com um label
 * "CyberSim — Etapa 1: pronto".
 *
 * O driver SDL e o LV_OS=NONE do lv_conf.h cuidam do plumbing — basta chamar
 * lv_sdl_window_create() e o loop principal vira lv_timer_handler() + sleep.
 * ============================================================================ */

#include <SDL2/SDL.h>
#include <stdio.h>

#include "lvgl.h"
#include "src/drivers/sdl/lv_sdl_window.h"

#define SIM_DISPLAY_W   480
#define SIM_DISPLAY_H   320

/* Tela inicial — substituida pelo splash do firmware na Etapa 4. */
static void build_hello_screen(void)
{
    lv_obj_t *scr = lv_screen_active();
    lv_obj_set_style_bg_color(scr, lv_color_hex(0x101418), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, LV_PART_MAIN);

    lv_obj_t *title = lv_label_create(scr);
    lv_label_set_text(title, "CyberSim — Etapa 1: pronto");
    lv_obj_set_style_text_color(title, lv_color_hex(0x00C853), LV_PART_MAIN);
    lv_obj_align(title, LV_ALIGN_CENTER, 0, -20);

    lv_obj_t *sub = lv_label_create(scr);
    lv_label_set_text(sub, "Etapas: SDL+LVGL OK | Input | FreeRTOS | Storage | Engine");
    lv_obj_set_style_text_color(sub, lv_color_hex(0x808890), LV_PART_MAIN);
    lv_obj_align(sub, LV_ALIGN_CENTER, 0, 10);

    lv_obj_t *hint = lv_label_create(scr);
    lv_label_set_text(hint, "Feche a janela para sair.");
    lv_obj_set_style_text_color(hint, lv_color_hex(0xFFC107), LV_PART_MAIN);
    lv_obj_align(hint, LV_ALIGN_BOTTOM_MID, 0, -10);
}

int main(int argc, char **argv)
{
    (void)argc; (void)argv;

    /* lv_init nao tem retorno em LVGL 9 — assume sucesso. */
    lv_init();

    /* Cria janela SDL + display LVGL. O LVGL bombeia eventos via timer
     * interno (LV_USE_OS=LV_OS_NONE), entao precisamos chamar
     * lv_timer_handler() periodicamente no loop. */
    lv_display_t *disp = lv_sdl_window_create(SIM_DISPLAY_W, SIM_DISPLAY_H);
    if (disp == NULL) {
        fprintf(stderr, "Falha ao criar janela SDL\n");
        return 1;
    }
    lv_sdl_window_set_title(disp, "CyberSim — Etapa 1");

    /* Mouse e teclado da janela como input devices (entram via lv_indev
     * nos proximos passos). Por enquanto apenas registramos pra capturar
     * eventos de janela (fechar). */
    lv_sdl_mouse_create();
    lv_sdl_keyboard_create();

    build_hello_screen();

    /* Loop principal: ~5 ms entre chamadas — bate com a periodicidade
     * sugerida pelo LVGL (lv_timer_handler retorna proximo deadline). */
    while (1) {
        uint32_t delay = lv_timer_handler();
        if (delay == LV_NO_TIMER_READY) delay = 5;
        if (delay > 100) delay = 100;
        SDL_Delay(delay);
    }
    return 0;
}
