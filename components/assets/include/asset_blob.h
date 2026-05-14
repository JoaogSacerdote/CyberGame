#pragma once
/* Contrato do header de blob de asset gravado na NAND.
 *
 * Cada asset no asset_store e armazenado como:
 *     [asset_blob_header_t (32 B)] [pixels crus]
 *
 * O asset_store guarda o blob inteiro como bytes opacos (size/crc/name no
 * manifest). O runtime loader (Fase 5) le este header e monta o
 * lv_image_dsc_t a partir dele.
 *
 * Espelhado em tools/asset_codec.py — manter os dois lados em sincronia.
 */

#include <stdint.h>

#define ASSET_BLOB_MAGIC      0x424C4241u  /* 'ABLB' em little-endian */
#define ASSET_BLOB_VERSION    1u

/* Formato de pixel — enum proprio, desacoplado do LV_COLOR_FORMAT_* do LVGL.
 * O loader mapeia para o cf do LVGL na hora de montar o lv_image_dsc_t. */
typedef enum {
    ASSET_PIXFMT_RGB565   = 0,  /* w*h*2 bytes                                */
    ASSET_PIXFMT_RGB565A8 = 1,  /* w*h*2 bytes RGB565 + w*h bytes A8 (depois) */
} asset_pixfmt_t;

typedef struct __attribute__((packed)) {
    uint32_t magic;          /* ASSET_BLOB_MAGIC                              */
    uint16_t version;        /* ASSET_BLOB_VERSION                            */
    uint16_t w;
    uint16_t h;
    uint16_t stride;         /* bytes por linha do plano RGB565 = w * 2       */
    int16_t  off_x;          /* offset de crop (bounding box dos pixels)      */
    int16_t  off_y;
    uint32_t data_size;      /* bytes de pixel apos o header                  */
    uint8_t  pixel_format;   /* asset_pixfmt_t                                */
    uint8_t  reserved[11];   /* zero                                          */
} asset_blob_header_t;       /* 32 bytes */

_Static_assert(sizeof(asset_blob_header_t) == 32,
               "asset_blob_header_t deve ter exatamente 32 bytes");
