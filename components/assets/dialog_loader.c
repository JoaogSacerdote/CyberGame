#include "dialog_loader.h"

#include <string.h>
#include "esp_heap_caps.h"
#include "esp_check.h"
#include "esp_log.h"

static const char *TAG = "DIALOG_LOADER";

esp_err_t dialog_loader_load(uint16_t id, dialog_t *out)
{
    ESP_RETURN_ON_FALSE(out != NULL, ESP_ERR_INVALID_ARG, TAG, "out NULL");
    memset(out, 0, sizeof(*out));

    /* 1. le e valida o header */
    dialog_blob_header_t hdr;
    ESP_RETURN_ON_ERROR(asset_store_read(ASSET_TYPE_SPRITE, id, 0, &hdr, sizeof(hdr)),
                        TAG, "dialog id=%u nao encontrado", id);

    if (hdr.magic != DIALOG_BLOB_MAGIC) {
        ESP_LOGE(TAG, "dialog id=%u: magic invalido 0x%08X", id, (unsigned)hdr.magic);
        return ESP_ERR_INVALID_RESPONSE;
    }
    if (hdr.version != DIALOG_BLOB_VERSION) {
        ESP_LOGE(TAG, "dialog id=%u: versao %u (suportada: %u)",
                 id, hdr.version, DIALOG_BLOB_VERSION);
        return ESP_ERR_INVALID_VERSION;
    }
    if (hdr.num_lines == 0 || hdr.num_lines > DIALOG_MAX_LINES) {
        ESP_LOGE(TAG, "dialog id=%u: num_lines %u fora da faixa (1..%u)",
                 id, hdr.num_lines, DIALOG_MAX_LINES);
        return ESP_ERR_INVALID_SIZE;
    }
    if (hdr.payload_size == 0) {
        ESP_LOGE(TAG, "dialog id=%u: payload_size zero", id);
        return ESP_ERR_INVALID_RESPONSE;
    }

    /* 2. confere tamanho total contra manifest */
    asset_info_t info;
    ESP_RETURN_ON_ERROR(asset_store_get_info(ASSET_TYPE_SPRITE, id, &info),
                        TAG, "get_info falhou dialog id=%u", id);

    const size_t offsets_size = (size_t)hdr.num_lines * sizeof(uint16_t);
    const size_t expected = sizeof(hdr) + offsets_size + hdr.payload_size;
    if (info.size != expected) {
        ESP_LOGE(TAG, "dialog id=%u: manifest %u B, esperado %u",
                 id, (unsigned)info.size, (unsigned)expected);
        return ESP_ERR_INVALID_RESPONSE;
    }

    /* 3. le os offsets (lista de uint16_t logo apos o header) */
    uint16_t offsets[DIALOG_MAX_LINES];
    ESP_RETURN_ON_ERROR(asset_store_read(ASSET_TYPE_SPRITE, id,
                                          sizeof(hdr), offsets, offsets_size),
                        TAG, "read offsets falhou");

    /* 4. valida que cada offset cai dentro do payload */
    for (uint16_t i = 0; i < hdr.num_lines; ++i) {
        if (offsets[i] >= hdr.payload_size) {
            ESP_LOGE(TAG, "dialog id=%u: offset[%u]=%u fora do payload (%u B)",
                     id, i, offsets[i], (unsigned)hdr.payload_size);
            return ESP_ERR_INVALID_RESPONSE;
        }
    }

    /* 5. aloca payload na PSRAM e le */
    char *buf = heap_caps_malloc(hdr.payload_size, MALLOC_CAP_SPIRAM);
    if (buf == NULL) {
        ESP_LOGE(TAG, "dialog id=%u: PSRAM insuficiente (%u B)",
                 id, (unsigned)hdr.payload_size);
        return ESP_ERR_NO_MEM;
    }

    const esp_err_t err = asset_store_read(ASSET_TYPE_SPRITE, id,
                                            sizeof(hdr) + offsets_size,
                                            buf, hdr.payload_size);
    if (err != ESP_OK) {
        heap_caps_free(buf);
        ESP_LOGE(TAG, "dialog id=%u: leitura do payload falhou: %s",
                 id, esp_err_to_name(err));
        return err;
    }

    /* 6. garante null terminator no final (defesa contra blob mal-formado) */
    if (buf[hdr.payload_size - 1] != '\0') {
        ESP_LOGW(TAG, "dialog id=%u: payload nao termina em '\\0' — forcando", id);
        buf[hdr.payload_size - 1] = '\0';
    }

    /* 7. monta os ponteiros */
    out->num_lines = hdr.num_lines;
    out->_buf      = buf;
    for (uint16_t i = 0; i < hdr.num_lines; ++i) {
        out->lines[i] = buf + offsets[i];
    }

    ESP_LOGI(TAG, "dialog id=%u carregado: %u linhas, %u B payload",
             id, hdr.num_lines, (unsigned)hdr.payload_size);
    return ESP_OK;
}

void dialog_loader_free(dialog_t *d)
{
    if (d == NULL || d->_buf == NULL) {
        return;
    }
    heap_caps_free(d->_buf);
    memset(d, 0, sizeof(*d));
}
