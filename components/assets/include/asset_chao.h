#pragma once
/* asset_chao.h — imagem de chao universal gravada em flash (EMBED_FILES).
 *
 * Diferente dos demais assets (carregados do SD), o CHAO.png e embarcado
 * diretamente no firmware para garantir que toda sala tenha chao mesmo sem
 * SD card e sem entrada no JSON de layout.
 *
 * Uso: lv_image_set_src(obj, asset_chao_get_dsc());
 */
#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Retorna ponteiro persistente para o lv_image_dsc_t do chao. Valido durante
 * toda a sessao — apontar diretamente para a flash via EMBED_FILES. */
const lv_image_dsc_t *asset_chao_get_dsc(void);

#ifdef __cplusplus
}
#endif
