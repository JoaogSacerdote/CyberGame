#pragma once

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    UI_SCREEN_NONE = 0,
    UI_SCREEN_SPLASH,
    UI_SCREEN_MENU,
    UI_SCREEN_PLACEHOLDER,   /* legacy — Etapa A; substituido por RECEPCAO/EMPRESA */
    UI_SCREEN_PAUSE,
    UI_SCREEN_RECEPCAO,
    UI_SCREEN_EMPRESA,
    UI_SCREEN_GAME_OVER,
} ui_screen_t;

/* ui_init: cria a tela ativa raiz com fundo neutro e prepara o roteador.
 * Pre-condicao: hal_bridge_init() ja rodou (LVGL pronto).
 * Idempotente.
 */
esp_err_t ui_init(void);

void ui_show_splash(void);
void ui_show_menu(void);
void ui_show_placeholder(void);   /* legacy — manter por compat */
void ui_show_pause(void);
void ui_show_recepcao(void);
void ui_show_empresa(void);
void ui_show_game_over(void);

ui_screen_t ui_get_active(void);

/* Cada screen_X.c implementa duas funcoes que o router chama: */
typedef void (*ui_screen_build_fn)(void);
typedef void (*ui_screen_destroy_fn)(void);

#ifdef __cplusplus
}
#endif
