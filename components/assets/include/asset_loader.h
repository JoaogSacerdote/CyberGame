#pragma once
/* Runtime loader de assets: le um blob da NAND (via asset_store), parseia o
 * asset_blob_header_t e monta um lv_image_dsc_t com os pixels na PSRAM.
 *
 * O asset_store e a camada de "arquivos na NAND" e NAO fala LVGL — esta
 * camada faz a ponte.
 *
 * CACHE load-once: cada asset e lido da NAND uma unica vez; chamadas
 * seguintes devolvem o mesmo buffer residente na PSRAM.
 *
 * OWNERSHIP: o cache eh SEMPRE o dono dos pixels. O caller recebe ponteiros
 * de leitura via lv_image_dsc_t mas NUNCA libera nada. asset_loader_free e
 * no-op explicito — fica na API por simetria com load(), mas nao faz nada.
 *
 * Pre-condicao: asset_store_init() ja rodou com sucesso.
 */

#include "lvgl.h"
#include "asset_store.h"   /* asset_type_t */
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    lv_image_dsc_t dsc;     /* pronto para lv_image_set_src()                  */
    int16_t        off_x;   /* offset de crop — somar na posicao de render     */
    int16_t        off_y;
} loaded_asset_t;

/* Preenche *out com o asset (type, id). Cache hit retorna instantaneo; cache
 * miss le da NAND e cacheia.
 *
 * Retornos de erro (so no caminho de cache miss):
 *   ESP_ERR_NOT_FOUND         — asset nao existe no manifest
 *   ESP_ERR_INVALID_RESPONSE  — header do blob corrompido/inconsistente
 *   ESP_ERR_INVALID_VERSION   — versao de blob nao suportada
 *   ESP_ERR_NO_MEM            — PSRAM insuficiente OU cache cheio
 * Em qualquer erro, *out fica zerado e nada precisa ser liberado. */
esp_err_t asset_loader_load(asset_type_t type, uint16_t id, loaded_asset_t *out);

/* No-op. Existe por simetria com asset_loader_load() — telas ainda chamam
 * load() no build e free() no destroy. Cache mantem os pixels residentes
 * pelo resto da sessao. */
void asset_loader_free(loaded_asset_t *a);

#ifdef __cplusplus
}
#endif
