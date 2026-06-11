#pragma once
/* Ícone de ataque vermelho (16x16 px, RGB565A8) gravado em flash.
 * Usado em screen_empresa.c para alertar ataque ativo em cada servidor. */
#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

const lv_image_dsc_t *asset_icone_vermelho_get_dsc(void);

#ifdef __cplusplus
}
#endif
