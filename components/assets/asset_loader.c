#include "asset_loader.h"
#include "asset_blob.h"

#include <string.h>
#include "esp_heap_caps.h"
#include "esp_check.h"
#include "esp_log.h"

static const char *TAG = "ASSET_LOADER";

/* ===================== Cache load-once =====================
 * Cada asset e lido da NAND uma unica vez e mantido residente na PSRAM pelo
 * resto da sessao. As telas continuam chamando asset_loader_load no build e
 * asset_loader_free no destroy normalmente — para assets cacheados o free e
 * no-op (loaded_asset_t._buf == NULL sinaliza "o caller nao e dono").
 *
 * Sem evicao: o MVP tem 17 assets (~2.2 MB) e 8 MB de PSRAM. */
#define ASSET_CACHE_MAX  32

typedef struct {
    bool           valid;
    asset_type_t   type;
    uint16_t       id;
    void          *buf;        /* dono real dos pixels na PSRAM */
    lv_image_dsc_t dsc;
    int16_t        off_x;
    int16_t        off_y;
} asset_cache_entry_t;

static asset_cache_entry_t s_cache[ASSET_CACHE_MAX];

static asset_cache_entry_t *cache_find(asset_type_t type, uint16_t id)
{
    for (int i = 0; i < ASSET_CACHE_MAX; ++i) {
        if (s_cache[i].valid && s_cache[i].type == type && s_cache[i].id == id) {
            return &s_cache[i];
        }
    }
    return NULL;
}

static asset_cache_entry_t *cache_alloc_slot(void)
{
    for (int i = 0; i < ASSET_CACHE_MAX; ++i) {
        if (!s_cache[i].valid) {
            return &s_cache[i];
        }
    }
    return NULL;
}

/* ===================== Decode do blob ===================== */

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

/* Le o blob (type,id) da NAND, valida o header, aloca os pixels na PSRAM e
 * preenche *dsc / *off_x / *off_y / *buf. Em sucesso, *buf e o dono dos
 * pixels alocados. */
static esp_err_t load_from_nand(asset_type_t type, uint16_t id,
                                lv_image_dsc_t *dsc, int16_t *off_x,
                                int16_t *off_y, void **buf)
{
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
    void *pixbuf = heap_caps_malloc(hdr.data_size, MALLOC_CAP_SPIRAM);
    if (pixbuf == NULL) {
        ESP_LOGE(TAG, "asset (%d,%u): PSRAM insuficiente para %u bytes",
                 type, id, (unsigned)hdr.data_size);
        return ESP_ERR_NO_MEM;
    }

    /* 3. le os pixels (vem logo apos o header de 32 B no blob) */
    const esp_err_t err = asset_store_read(type, id, sizeof(hdr), pixbuf, hdr.data_size);
    if (err != ESP_OK) {
        heap_caps_free(pixbuf);
        ESP_LOGE(TAG, "asset (%d,%u): falha lendo pixels: %s",
                 type, id, esp_err_to_name(err));
        return err;
    }

    /* 4. monta o lv_image_dsc_t apontando para o buffer PSRAM */
    memset(dsc, 0, sizeof(*dsc));
    dsc->header.magic  = LV_IMAGE_HEADER_MAGIC;
    dsc->header.cf     = cf;
    dsc->header.flags  = 0;
    dsc->header.w      = hdr.w;
    dsc->header.h      = hdr.h;
    dsc->header.stride = hdr.stride;
    dsc->data_size     = hdr.data_size;
    dsc->data          = (const uint8_t *)pixbuf;
    *off_x = hdr.off_x;
    *off_y = hdr.off_y;
    *buf   = pixbuf;

    ESP_LOGD(TAG, "asset (%d,%u) lido da NAND: %ux%u cf=%d off=%d,%d %u B",
             type, id, hdr.w, hdr.h, (int)cf, hdr.off_x, hdr.off_y,
             (unsigned)hdr.data_size);
    return ESP_OK;
}

/* ===================== API publica ===================== */

esp_err_t asset_loader_load(asset_type_t type, uint16_t id, loaded_asset_t *out)
{
    ESP_RETURN_ON_FALSE(out != NULL, ESP_ERR_INVALID_ARG, TAG, "out NULL");
    memset(out, 0, sizeof(*out));

    /* Cache hit: devolve apontando para o buffer residente. O caller NAO e
     * dono — _buf fica NULL e asset_loader_free vira no-op. */
    asset_cache_entry_t *c = cache_find(type, id);
    if (c != NULL) {
        out->dsc   = c->dsc;
        out->off_x = c->off_x;
        out->off_y = c->off_y;
        out->_buf  = NULL;
        return ESP_OK;
    }

    /* Cache miss: le da NAND. */
    lv_image_dsc_t dsc;
    int16_t off_x = 0, off_y = 0;
    void *buf = NULL;
    const esp_err_t err = load_from_nand(type, id, &dsc, &off_x, &off_y, &buf);
    if (err != ESP_OK) {
        return err;
    }

    asset_cache_entry_t *slot = cache_alloc_slot();
    if (slot != NULL) {
        slot->valid = true;
        slot->type  = type;
        slot->id    = id;
        slot->buf   = buf;
        slot->dsc   = dsc;
        slot->off_x = off_x;
        slot->off_y = off_y;
        out->dsc   = dsc;
        out->off_x = off_x;
        out->off_y = off_y;
        out->_buf  = NULL;            /* dono e o cache */
    } else {
        /* Cache cheio (nao deveria acontecer no MVP): o caller vira dono do
         * buffer e asset_loader_free o liberara normalmente. */
        ESP_LOGW(TAG, "cache cheio (%d slots) — asset (%d,%u) sem cache",
                 ASSET_CACHE_MAX, type, id);
        out->dsc   = dsc;
        out->off_x = off_x;
        out->off_y = off_y;
        out->_buf  = buf;
    }
    return ESP_OK;
}

void asset_loader_free(loaded_asset_t *a)
{
    /* Para assets cacheados (o caso normal) _buf == NULL e isto e no-op —
     * o cache mantem os pixels residentes na PSRAM (load-once). So libera o
     * caso de fallback de cache cheio. */
    if (a == NULL || a->_buf == NULL) {
        return;
    }
    heap_caps_free(a->_buf);
    memset(a, 0, sizeof(*a));
}
