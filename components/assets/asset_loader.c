#include "asset_loader.h"
#include "asset_blob.h"

#include <stdio.h>
#include <string.h>
#include "esp_heap_caps.h"
#include "esp_check.h"
#include "esp_log.h"

static const char *TAG = "ASSET_LOADER";

/* Diretorio dos assets no cartao microSD (montado por sd_hal em /sd).
 * Cada asset e um arquivo "<type>_<id>.bin" com layout
 * [asset_blob_header_t 32B][pixels]. */
#define ASSET_SD_DIR  "/sd/assets"

/* ===================== Cache load-once =====================
 * Cada asset e lido do cartao uma unica vez e mantido residente na PSRAM
 * pelo resto da sessao. O cache eh SEMPRE o dono dos pixels — callers
 * recebem lv_image_dsc_t com ponteiros somente-leitura e nunca liberam nada.
 *
 * Sem evicao: o MVP tem ~18 assets (~2.2 MB) e 8 MB de PSRAM. Se exceder
 * ASSET_CACHE_MAX, load() retorna ESP_ERR_NO_MEM. */
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
 * LVGL. Desacopla o formato gravado em disco da versao do LVGL. */
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

/* Le o arquivo do asset (type,id) do cartao, valida o header, aloca os pixels
 * na PSRAM e preenche *dsc / *off_x / *off_y / *buf. Em sucesso, *buf e o dono
 * dos pixels alocados. */
static esp_err_t load_from_sd(asset_type_t type, uint16_t id,
                              lv_image_dsc_t *dsc, int16_t *off_x,
                              int16_t *off_y, void **buf)
{
    char path[48];
    snprintf(path, sizeof(path), ASSET_SD_DIR "/%d_%u.bin", (int)type, (unsigned)id);

    FILE *f = fopen(path, "rb");
    if (f == NULL) {
        ESP_LOGE(TAG, "asset (%d,%u): nao abriu %s", type, id, path);
        return ESP_ERR_NOT_FOUND;
    }

    /* 1. le e valida o header do blob */
    asset_blob_header_t hdr;
    if (fread(&hdr, 1, sizeof(hdr), f) != sizeof(hdr)) {
        fclose(f);
        ESP_LOGE(TAG, "asset (%d,%u): header curto/ilegivel", type, id);
        return ESP_ERR_INVALID_SIZE;
    }

    if (hdr.magic != ASSET_BLOB_MAGIC) {
        fclose(f);
        ESP_LOGE(TAG, "asset (%d,%u): magic invalido 0x%08X",
                 type, id, (unsigned)hdr.magic);
        return ESP_ERR_INVALID_RESPONSE;
    }
    if (hdr.version != ASSET_BLOB_VERSION) {
        fclose(f);
        ESP_LOGE(TAG, "asset (%d,%u): versao de blob %u (suportada: %u)",
                 type, id, hdr.version, ASSET_BLOB_VERSION);
        return ESP_ERR_INVALID_VERSION;
    }
    const lv_color_format_t cf = pixfmt_to_lv(hdr.pixel_format);
    if (cf == LV_COLOR_FORMAT_UNKNOWN) {
        fclose(f);
        ESP_LOGE(TAG, "asset (%d,%u): pixel_format %u desconhecido",
                 type, id, hdr.pixel_format);
        return ESP_ERR_INVALID_RESPONSE;
    }
    if (expected_pixels(hdr.w, hdr.h, hdr.pixel_format) != hdr.data_size) {
        fclose(f);
        ESP_LOGE(TAG, "asset (%d,%u): data_size %u incoerente com %ux%u fmt %u",
                 type, id, (unsigned)hdr.data_size, hdr.w, hdr.h, hdr.pixel_format);
        return ESP_ERR_INVALID_RESPONSE;
    }

    /* 1b. confere o tamanho total do arquivo == header + pixels */
    if (fseek(f, 0, SEEK_END) != 0) {
        fclose(f);
        return ESP_ERR_INVALID_RESPONSE;
    }
    const long fsize = ftell(f);
    if (fsize != (long)(sizeof(hdr) + hdr.data_size)) {
        fclose(f);
        ESP_LOGE(TAG, "asset (%d,%u): arquivo %ld B, esperado %u",
                 type, id, fsize, (unsigned)(sizeof(hdr) + hdr.data_size));
        return ESP_ERR_INVALID_RESPONSE;
    }

    /* 2. aloca os pixels: prefere PSRAM, cai pra heap interna se PSRAM
     * desligada (modo bring-up) ou cheia. */
    void *pixbuf = heap_caps_malloc(hdr.data_size, MALLOC_CAP_SPIRAM);
    if (pixbuf == NULL) {
        pixbuf = heap_caps_malloc(hdr.data_size, MALLOC_CAP_8BIT);
    }
    if (pixbuf == NULL) {
        fclose(f);
        ESP_LOGE(TAG, "asset (%d,%u): sem memoria para %u bytes (PSRAM+DRAM esgotadas)",
                 type, id, (unsigned)hdr.data_size);
        return ESP_ERR_NO_MEM;
    }

    /* 3. le os pixels (vem logo apos o header de 32 B) */
    if (fseek(f, sizeof(hdr), SEEK_SET) != 0 ||
        fread(pixbuf, 1, hdr.data_size, f) != hdr.data_size) {
        heap_caps_free(pixbuf);
        fclose(f);
        ESP_LOGE(TAG, "asset (%d,%u): falha lendo pixels", type, id);
        return ESP_ERR_INVALID_SIZE;
    }
    fclose(f);

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

    ESP_LOGD(TAG, "asset (%d,%u) lido do SD: %ux%u cf=%d off=%d,%d %u B",
             type, id, hdr.w, hdr.h, (int)cf, hdr.off_x, hdr.off_y,
             (unsigned)hdr.data_size);
    return ESP_OK;
}

/* ===================== API publica ===================== */

esp_err_t asset_loader_load(asset_type_t type, uint16_t id, loaded_asset_t *out)
{
    ESP_RETURN_ON_FALSE(out != NULL, ESP_ERR_INVALID_ARG, TAG, "out NULL");
    memset(out, 0, sizeof(*out));

    /* Cache hit: devolve apontando pro buffer residente. */
    asset_cache_entry_t *c = cache_find(type, id);
    if (c != NULL) {
        out->dsc   = c->dsc;
        out->off_x = c->off_x;
        out->off_y = c->off_y;
        return ESP_OK;
    }

    /* Cache miss: precisa de slot ANTES de ler. Se nao ha slot, falha rapido
     * sem desperdicar PSRAM lendo um asset que nao vai ficar acessivel. */
    asset_cache_entry_t *slot = cache_alloc_slot();
    if (slot == NULL) {
        ESP_LOGE(TAG, "cache cheio (%d slots) — asset (%d,%u) nao pode ser carregado",
                 ASSET_CACHE_MAX, type, id);
        return ESP_ERR_NO_MEM;
    }

    /* Le do cartao e cacheia. */
    lv_image_dsc_t dsc;
    int16_t off_x = 0, off_y = 0;
    void *buf = NULL;
    const esp_err_t err = load_from_sd(type, id, &dsc, &off_x, &off_y, &buf);
    if (err != ESP_OK) {
        return err;
    }

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
    return ESP_OK;
}

void asset_loader_free(loaded_asset_t *a)
{
    /* No-op explicito. Cache eh dono — pixels ficam residentes na PSRAM
     * pelo resto da sessao. Funcao existe por simetria com load(). */
    (void)a;
}

const lv_image_dsc_t *asset_loader_get_dsc(asset_type_t type, uint16_t id)
{
    asset_cache_entry_t *c = cache_find(type, id);
    return c ? &c->dsc : NULL;
}
