#pragma once

/* Interfaces internas entre o router e cada screen_X.c.
 * Nao expostas publicamente. */

#include "lvgl.h"
#include "button_hal.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Cada screen exporta um par build/destroy. O router chama destroy da tela
 * atual antes de build da nova. */
void screen_splash_build(void);
void screen_splash_destroy(void);

void screen_menu_build(void);
void screen_menu_destroy(void);

void screen_placeholder_build(void);
void screen_placeholder_destroy(void);

void screen_pause_build(void);
void screen_pause_destroy(void);

void screen_recepcao_build(void);
void screen_recepcao_destroy(void);

void screen_empresa_build(void);
void screen_empresa_destroy(void);

void screen_game_over_build(void);
void screen_game_over_destroy(void);

/* HUD: barra superior persistente em recepcao/empresa. Construido como
 * filho da tela atual; nao tem ciclo de vida proprio no router. */
#define UI_HUD_HEIGHT_PX  32
void screen_hud_build(lv_obj_t *parent);
void screen_hud_destroy(void);
void screen_hud_tick(void);

/* Helper compartilhado: detecta edge RELEASED->PRESSED para um botao.
 * O state cache fica por conta de cada screen. */
static inline bool ui_btn_edge(button_id_t id, button_state_t *cache)
{
    const button_state_t now = button_hal_peek(id);
    const bool edge = (now == BTN_PRESSED && *cache == BTN_RELEASED);
    *cache = now;
    return edge;
}

/* Cor base para todas as telas. */
#define UI_COLOR_BG       lv_color_hex(0x101418)
#define UI_COLOR_TEXT     lv_color_hex(0xE6E6E6)
#define UI_COLOR_DIM      lv_color_hex(0x808890)
#define UI_COLOR_ACCENT   lv_color_hex(0x00C853)
#define UI_COLOR_WARN     lv_color_hex(0xFFC107)

/* Periodo padrao dos timers de refresh de tela (igual ao ui_debug). */
#define UI_TICK_MS                  100

/* Navegacao do menu — locais (proprios da UI, sem depender do engine). */
#define UI_MENU_NAV_DEBOUNCE_MS     300
#define UI_JOYSTICK_DEFLEXAO_MIN    50

#ifdef __cplusplus
}
#endif
