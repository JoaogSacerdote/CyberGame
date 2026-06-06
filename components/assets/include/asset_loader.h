#pragma once
/* Runtime loader de assets: le um blob do cartao microSD (via FATFS), parseia
 * o asset_blob_header_t e monta um lv_image_dsc_t com os pixels na PSRAM.
 *
 * CACHE load-once: cada asset e lido do SD uma unica vez; chamadas seguintes
 * devolvem o mesmo buffer residente na PSRAM.
 *
 * OWNERSHIP: o cache eh SEMPRE o dono dos pixels. O caller recebe ponteiros
 * de leitura via lv_image_dsc_t mas NUNCA libera nada. asset_loader_free e
 * no-op explicito — fica na API por simetria com load(), mas nao faz nada.
 */

#include "lvgl.h"
#include "esp_err.h"

typedef enum {
    ASSET_TYPE_SPRITE = 0,
    ASSET_TYPE_FONT,
    ASSET_TYPE_SOUND,
    ASSET_TYPE_MAX,
} asset_type_t;

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    lv_image_dsc_t dsc;     /* pronto para lv_image_set_src()                  */
    int16_t        off_x;   /* offset de crop — somar na posicao de render     */
    int16_t        off_y;
} loaded_asset_t;

/* Preenche *out com o asset (type, id). Cache hit retorna instantaneo; cache
 * miss le do SD card e cacheia.
 *
 * Retornos de erro (so no caminho de cache miss):
 *   ESP_ERR_NOT_FOUND         — arquivo /sd/assets/<type>_<id>.bin nao existe
 *   ESP_ERR_INVALID_RESPONSE  — header do blob corrompido/inconsistente
 *   ESP_ERR_INVALID_VERSION   — versao de blob nao suportada
 *   ESP_ERR_NO_MEM            — PSRAM insuficiente OU cache cheio
 * Em qualquer erro, *out fica zerado e nada precisa ser liberado. */
esp_err_t asset_loader_load(asset_type_t type, uint16_t id, loaded_asset_t *out);

/* No-op. Existe por simetria com asset_loader_load() — telas ainda chamam
 * load() no build e free() no destroy. Cache mantem os pixels residentes
 * pelo resto da sessao. */
void asset_loader_free(loaded_asset_t *a);

/* Retorna ponteiro PERSISTENTE para o lv_image_dsc_t que vive dentro do cache.
 * Valido enquanto o asset estiver no cache (sessao inteira).
 * DEVE ser chamado apos asset_loader_load() ter retornado ESP_OK para o mesmo
 * (type, id) — retorna NULL se o asset nao estiver no cache.
 * Use este ponteiro em lv_image_set_src() para entidades que sobrevivem alem
 * da funcao que fez o load(). */
const lv_image_dsc_t *asset_loader_get_dsc(asset_type_t type, uint16_t id);

#ifdef __cplusplus
}
#endif
