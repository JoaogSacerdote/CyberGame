#pragma once
/* Runtime loader de assets: le um blob da NAND (via asset_store), parseia o
 * asset_blob_header_t e monta um lv_image_dsc_t com os pixels na PSRAM.
 *
 * O asset_store e a camada de "arquivos na NAND" e NAO fala LVGL — esta
 * camada faz a ponte. O caller e dono da memoria alocada: chamar
 * asset_loader_free() ao descartar o asset (tipicamente no destroy da tela).
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
    void          *_buf;    /* buffer PSRAM dono dos pixels (uso interno)      */
} loaded_asset_t;

/* Carrega o asset (type, id) da NAND para a PSRAM e preenche *out.
 *
 * Retornos de erro:
 *   ESP_ERR_NOT_FOUND         — asset nao existe no manifest
 *   ESP_ERR_INVALID_RESPONSE  — header do blob corrompido/inconsistente
 *   ESP_ERR_INVALID_VERSION   — versao de blob nao suportada
 *   ESP_ERR_NO_MEM            — PSRAM insuficiente
 * Em qualquer erro, *out fica zerado e nada precisa ser liberado. */
esp_err_t asset_loader_load(asset_type_t type, uint16_t id, loaded_asset_t *out);

/* Libera os pixels e zera a struct. Seguro chamar com *a ja zerado ou NULL. */
void asset_loader_free(loaded_asset_t *a);

#ifdef __cplusplus
}
#endif
