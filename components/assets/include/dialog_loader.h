#pragma once
/* Runtime loader de blobs de dialogo. Le um blob da NAND (via asset_store),
 * valida o header, aloca o payload na PSRAM e monta um array de ponteiros
 * pras strings null-terminated.
 *
 * Diferente do asset_loader (que retorna LVGL image dsc), aqui devolvemos
 * apenas (const char*)[]. Sem cache global — cada load aloca, cada free
 * libera. Como dialogos sao pequenos (~600 B) e raros (1 por NPC), nao
 * vale o complexidade extra.
 *
 * Pre-condicao: asset_store_init() ja rodou com sucesso.
 */

#include <stdint.h>
#include "asset_store.h"
#include "dialog_blob.h"
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uint16_t      num_lines;
    const char   *lines[DIALOG_MAX_LINES];  /* aponta pra dentro de _buf */
    void         *_buf;                     /* PSRAM dono do payload     */
} dialog_t;

/* Carrega o dialogo (tipo SPRITE, id) da NAND. Em sucesso, *out tem
 * num_lines/lines/_buf preenchidos. Em erro, *out fica zerado.
 *
 * Retornos de erro:
 *   ESP_ERR_NOT_FOUND          — asset nao existe no manifest
 *   ESP_ERR_INVALID_RESPONSE   — header corrompido (magic/payload_size)
 *   ESP_ERR_INVALID_VERSION    — versao de blob nao suportada
 *   ESP_ERR_INVALID_SIZE       — num_lines > DIALOG_MAX_LINES
 *   ESP_ERR_NO_MEM             — PSRAM insuficiente */
esp_err_t dialog_loader_load(uint16_t id, dialog_t *out);

/* Libera o payload PSRAM e zera *d. Seguro com d == NULL ou *d zerado. */
void dialog_loader_free(dialog_t *d);

#ifdef __cplusplus
}
#endif
