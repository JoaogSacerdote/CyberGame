#include "asset_loader.h"
#include "asset_blob.h"

#include <string.h>
#include "esp_heap_caps.h"
#include "esp_check.h"
#include "esp_log.h"

static const char *TAG = "ASSET_LOADER";

/* Mapeia o pixel_format do blob (enum proprio) para o LV_COLOR_FORMAT_* do
 * LVGL. Desacopla o formato gravado na NAND da versao do LVGL. */
static lv_color_format_t pixfmt_to_lv(uint8_t pixfmt)
{
    switch (pixfmt) {
    case ASSET_PIXFMT_RGB565:   return LV_COLOR_FORMAT_RGB565;
    case ASSET_PIXFMT_RGB565A8: return LV_COLOR_FORMAT_RGB565A8;
    default:                    return LV_COLOR_FORMAT_UNKNOWN;
    }
}

/* Tamanho esperado dos pixels para w/h/formato — usado pra pegar blob corrompido. */
static size_t expected_pixels(uint16_t w, uint16_t h, uint8_t pixfmt)
{
    size_t n = (size_t)w * h * 2;                 /* plano RGB565       */
    if (pixfmt == ASSET_PIXFMT_RGB565A8) {
        n += (size_t)w * h;                       /* plano A8 contiguo  */
    }
    return n;
}

esp_err_t asset_loader_load(asset_type_t type, uint16_t id, loaded_asset_t *out)
{
    ESP_RETURN_ON_FALSE(out != NULL, ESP_ERR_INVALID_ARG, TAG, "out NULL");
    memset(out, 0, sizeof(*out));

    /* 1. le e valida o header do blob */
    asset_blob_header_t hdr;
    ESP_RETURN_ON_ERROR(asset_store_read(type, id, 0, &hdr, sizeof(hdr)),
                        TAG, "asset (%d,%u) nao encontrado", type, id);

    if (hdr.magic != ASSET_BLOB_MAGIC) {
        ESP_LOGE(TAG, "asset (%d,%u): magic invalido 0x%08X",
                 type, id, (unsigned)hdr.magic);
        return ESP_ERR_INVALID_RESPONSE;
    }
    if (hdr.version != ASSET_BLOB_VERSION) {
        ESP_LOGE(TAG, "asset (%d,%u): versao de blob %u (suportada: %u)",
                 type, id, hdr.version, ASSET_BLOB_VERSION);
        return ESP_ERR_INVALID_VERSION;
    }
    const lv_color_format_t cf = pixfmt_to_lv(hdr.pixel_format);
    if (cf == LV_COLOR_FORMAT_UNKNOWN) {
        ESP_LOGE(TAG, "asset (%d,%u): pixel_format %u desconhecido",
                 type, id, hdr.pixel_format);
        return ESP_ERR_INVALID_RESPONSE;
    }
    if (expected_pixels(hdr.w, hdr.h, hdr.pixel_format) != hdr.data_size) {
        ESP_LOGE(TAG, "asset (%d,%u): data_size %u incoerente com %ux%u fmt %u",
                 type, id, (unsigned)hdr.data_size, hdr.w, hdr.h, hdr.pixel_format);
        return ESP_ERR_INVALID_RESPONSE;
    }

    /* 1b. confere o tamanho total contra o manifest do asset_store */
    asset_info_t info;
    ESP_RETURN_ON_ERROR(asset_store_get_info(type, id, &info),
                        TAG, "get_info falhou para asset (%d,%u)", type, id);
    if (info.size != sizeof(hdr) + hdr.data_size) {
        ESP_LOGE(TAG, "asset (%d,%u): manifest diz %u B, header diz %u+%u",
                 type, id, (unsigned)info.size,
                 (unsigned)sizeof(hdr), (unsigned)hdr.data_size);
        return ESP_ERR_INVALID_RESPONSE;
    }

    /* 2. aloca os pixels na PSRAM */
    void *buf = heap_caps_malloc(hdr.data_size, MALLOC_CAP_SPIRAM);
    if (buf == NULL) {
        ESP_LOGE(TAG, "asset (%d,%u): PSRAM insuficiente para %u bytes",
                 type, id, (unsigned)hdr.data_size);
        return ESP_ERR_NO_MEM;
    }

    /* 3. le os pixels (vem logo apos o header de 32 B no blob) */
    const esp_err_t err = asset_store_read(type, id, sizeof(hdr), buf, hdr.data_size);
    if (err != ESP_OK) {
        heap_caps_free(buf);
        ESP_LOGE(TAG, "asset (%d,%u): falha lendo pixels: %s",
                 type, id, esp_err_to_name(err));
        return err;
    }

    /* 4. monta o lv_image_dsc_t apontando para o buffer PSRAM */
    out->_buf  = buf;
    out->off_x = hdr.off_x;
    out->off_y = hdr.off_y;
    out->dsc.header.magic  = LV_IMAGE_HEADER_MAGIC;
    out->dsc.header.cf     = cf;
    out->dsc.header.flags  = 0;
    out->dsc.header.w      = hdr.w;
    out->dsc.header.h      = hdr.h;
    out->dsc.header.stride = hdr.stride;
    out->dsc.data_size     = hdr.data_size;
    out->dsc.data          = (const uint8_t *)buf;

    ESP_LOGD(TAG, "asset (%d,%u) carregado: %ux%u cf=%d off=%d,%d %u B na PSRAM",
             type, id, hdr.w, hdr.h, (int)cf, hdr.off_x, hdr.off_y,
             (unsigned)hdr.data_size);
    return ESP_OK;
}

void asset_loader_free(loaded_asset_t *a)
{
    if (a == NULL || a->_buf == NULL) {
        return;
    }
    heap_caps_free(a->_buf);
    memset(a, 0, sizeof(*a));
}
