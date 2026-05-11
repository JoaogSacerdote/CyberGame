#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Camada de "arquivos" sobre storage_hal (NAND W25N01GV).
 *
 * Layout fixo por categoria:
 *   Bloco       0     reserva
 *   Bloco       1-3   manifest (3 copias rotativas)
 *   Bloco       4-99  SPRITES   (96 blocos = 12 MB)
 *   Bloco     100-149 FONTES    (50 blocos = 6.25 MB)
 *   Bloco     150-199 SONS      (50 blocos = 6.25 MB)
 *   Bloco     200-1022 RESERVA
 *   Bloco       1023  POST destrutivo (ja usado por storage_hal)
 *
 * Pre-condicao: storage_hal_init() ja rodou com sucesso.
 *
 * Etapa 1 (atual): API read-only + listagem. Write API vira na Etapa 2.
 */

typedef enum {
    ASSET_TYPE_SPRITE = 0,
    ASSET_TYPE_FONT,
    ASSET_TYPE_SOUND,
    ASSET_TYPE_MAX,
} asset_type_t;

#define ASSET_NAME_MAX     40

typedef struct {
    asset_type_t type;
    uint16_t     id;
    uint32_t     size;          /* bytes uteis do payload */
    uint32_t     crc;           /* CRC32 LE do payload */
    char         name[ASSET_NAME_MAX];
} asset_info_t;

/* Carrega o manifest da NAND (tenta as 3 copias e fica com a de maior
 * generation que passe a validacao). Se nenhuma copia for valida, comeca
 * com manifest vazio (cold start) — sem escrever na NAND ate a Etapa 2. */
esp_err_t asset_store_init(void);

/* Lookup de metadata. Retorna ESP_ERR_NOT_FOUND se (type,id) nao existe. */
esp_err_t asset_store_get_info(asset_type_t type, uint16_t id, asset_info_t *out);

/* Le 'size' bytes do asset (type, id) comecando em 'offset', para 'buf'.
 * Retorna ESP_ERR_NOT_FOUND se nao existe; ESP_ERR_INVALID_ARG se
 * offset+size > tamanho do asset. NAO valida CRC (cabe ao caller, se quiser). */
esp_err_t asset_store_read(asset_type_t type, uint16_t id,
                           size_t offset, void *buf, size_t size);

/* Numero de entradas validas no manifest. */
esp_err_t asset_store_count(size_t *out_count);

/* Copia ate 'max' entradas para out[]. *actual recebe quantas foram copiadas. */
esp_err_t asset_store_list(asset_info_t *out, size_t max, size_t *actual);

#ifdef __cplusplus
}
#endif
