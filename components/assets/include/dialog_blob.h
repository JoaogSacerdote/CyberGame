#pragma once
/* Contrato do header de blob de dialogo gravado no cartao microSD.
 *
 * Cada dialogo vive em /sd/assets/0_<id>.bin (mesmo schema de path dos
 * sprites) com o seguinte layout:
 *     [dialog_blob_header_t (12 B)]
 *     [uint16_t offsets[num_lines]]   -- offset de cada linha dentro do payload
 *     [payload]                       -- strings null-terminated concatenadas
 *
 * O dialog_loader le este header pelo FATFS, valida, aloca o payload na
 * PSRAM e monta um array de const char* apontando pras strings.
 *
 * Espelhado em tools/asset_codec.py (build_dialog_blob) — manter os dois lados
 * em sincronia.
 */

#include <stdint.h>

#define DIALOG_BLOB_MAGIC      0x42474C44u  /* 'DLGB' em little-endian */
#define DIALOG_BLOB_VERSION    1u
#define DIALOG_MAX_LINES       16u

typedef struct __attribute__((packed)) {
    uint32_t magic;            /* DIALOG_BLOB_MAGIC                       */
    uint16_t version;          /* DIALOG_BLOB_VERSION                     */
    uint16_t num_lines;        /* <= DIALOG_MAX_LINES                     */
    uint32_t payload_size;     /* bytes do payload (com null terminators) */
} dialog_blob_header_t;        /* 12 bytes */

_Static_assert(sizeof(dialog_blob_header_t) == 12,
               "dialog_blob_header_t deve ter exatamente 12 bytes");
