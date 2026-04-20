/**
 * =============================================================================
 * CyberSec: Network Defender — LVGL PC Simulator
 * Arquivo de cabeçalho público
 * =============================================================================
 *
 * Como integrar ao lv_port_pc (template SDL):
 *   1. Copie cybersec_game.h e cybersec_game.c para main/src/
 *   2. No seu main.c, após lv_init() e lv_port_disp_init(), chame:
 *          cybersec_start();
 *   3. Na SDL event loop, passe eventos de teclado via:
 *          cybersec_sdl_key_event(sdl_key, is_down);
 *
 * Resolução alvo: 320 x 240 px (ILI9341)
 * LVGL versão:   >= 8.3
 * =============================================================================
 */

#ifndef CYBERSEC_GAME_H
#define CYBERSEC_GAME_H

#include "lvgl/lvgl.h"
#include <stdbool.h>
#include <stdint.h>

/* --- API pública --- */

/**
 * @brief Inicializa e exibe o jogo na tela LVGL ativa.
 *        Chame uma vez após lv_init().
 */
void cybersec_start(void);

/**
 * @brief Passa um evento de teclado SDL para o jogo.
 *
 * @param sdlk   Código de tecla SDL (SDL_Keycode), ex: SDLK_UP, SDLK_SPACE
 * @param is_down true = pressionada, false = solta
 *
 * Teclas reconhecidas:
 *   SDLK_UP/DOWN/LEFT/RIGHT  → movimento
 *   SDLK_SPACE, SDLK_RETURN  → interagir com sala
 *   SDLK_n                   → simular scan NFC
 *   SDLK_r                   → reiniciar jogo
 */
void cybersec_sdl_key_event(int32_t sdlk, bool is_down);

#endif /* CYBERSEC_GAME_H */