#include "dialog_loader.h"
#include "asset_loader.h"   /* asset_type_t / ASSET_TYPE_SPRITE */

#include <stdio.h>
#include <string.h>
#include "esp_heap_caps.h"
#include "esp_check.h"
#include "esp_log.h"

static const char *TAG = "DIALOG_LOADER";

/* Mesmo diretorio dos demais assets no cartao (ver asset_loader.c). O dialogo
 * e o asset (SPRITE, id) gravado como /sd/assets/<type>_<id>.bin. */
#define ASSET_SD_DIR  "/sd/assets"

esp_err_t dialog_loader_load(uint16_t id, dialog_t *out)
{
    ESP_RETURN_ON_FALSE(out != NULL, ESP_ERR_INVALID_ARG, TAG, "out NULL");
    memset(out, 0, sizeof(*out));

    char path[48];
    snprintf(path, sizeof(path), ASSET_SD_DIR "/%d_%u.bin",
             (int)ASSET_TYPE_SPRITE, (unsigned)id);

    FILE *f = fopen(path, "rb");
    if (f == NULL) {
        ESP_LOGE(TAG, "dialog id=%u: nao abriu %s", id, path);
        return ESP_ERR_NOT_FOUND;
    }

    /* 1. le e valida o header */
    dialog_blob_header_t hdr;
    if (fread(&hdr, 1, sizeof(hdr), f) != sizeof(hdr)) {
        fclose(f);
        ESP_LOGE(TAG, "dialog id=%u: header curto/ilegivel", id);
        return ESP_ERR_INVALID_SIZE;
    }

    if (hdr.magic != DIALOG_BLOB_MAGIC) {
        fclose(f);
        ESP_LOGE(TAG, "dialog id=%u: magic invalido 0x%08X", id, (unsigned)hdr.magic);
        return ESP_ERR_INVALID_RESPONSE;
    }
    if (hdr.version != DIALOG_BLOB_VERSION) {
        fclose(f);
        ESP_LOGE(TAG, "dialog id=%u: versao %u (suportada: %u)",
                 id, hdr.version, DIALOG_BLOB_VERSION);
        return ESP_ERR_INVALID_VERSION;
    }
    if (hdr.num_lines == 0 || hdr.num_lines > DIALOG_MAX_LINES) {
        fclose(f);
        ESP_LOGE(TAG, "dialog id=%u: num_lines %u fora da faixa (1..%u)",
                 id, hdr.num_lines, DIALOG_MAX_LINES);
        return ESP_ERR_INVALID_SIZE;
    }
    if (hdr.payload_size == 0) {
        fclose(f);
        ESP_LOGE(TAG, "dialog id=%u: payload_size zero", id);
        return ESP_ERR_INVALID_RESPONSE;
    }

    const size_t offsets_size = (size_t)hdr.num_lines * sizeof(uint16_t);

    /* 2. confere o tamanho total do arquivo */
    if (fseek(f, 0, SEEK_END) != 0) {
        fclose(f);
        return ESP_ERR_INVALID_RESPONSE;
    }
    const long fsize = ftell(f);
    const size_t expected = sizeof(hdr) + offsets_size + hdr.payload_size;
    if (fsize != (long)expected) {
        fclose(f);
        ESP_LOGE(TAG, "dialog id=%u: arquivo %ld B, esperado %u",
                 id, fsize, (unsigned)expected);
        return ESP_ERR_INVALID_RESPONSE;
    }

    /* 3. le os offsets (lista de uint16_t logo apos o header) */
    uint16_t offsets[DIALOG_MAX_LINES];
    if (fseek(f, sizeof(hdr), SEEK_SET) != 0 ||
        fread(offsets, 1, offsets_size, f) != offsets_size) {
        fclose(f);
        ESP_LOGE(TAG, "dialog id=%u: leitura dos offsets falhou", id);
        return ESP_ERR_INVALID_SIZE;
    }

    /* 4. valida que cada offset cai dentro do payload */
    for (uint16_t i = 0; i < hdr.num_lines; ++i) {
        if (offsets[i] >= hdr.payload_size) {
            fclose(f);
            ESP_LOGE(TAG, "dialog id=%u: offset[%u]=%u fora do payload (%u B)",
                     id, i, offsets[i], (unsigned)hdr.payload_size);
            return ESP_ERR_INVALID_RESPONSE;
        }
    }

    /* 5. aloca payload: prefere PSRAM, cai pra DRAM se PSRAM desligada (bring-up). */
    char *buf = heap_caps_malloc(hdr.payload_size, MALLOC_CAP_SPIRAM);
    if (buf == NULL) {
        buf = heap_caps_malloc(hdr.payload_size, MALLOC_CAP_8BIT);
    }
    if (buf == NULL) {
        fclose(f);
        ESP_LOGE(TAG, "dialog id=%u: sem memoria (%u B; PSRAM+DRAM esgotadas)",
                 id, (unsigned)hdr.payload_size);
        return ESP_ERR_NO_MEM;
    }

    if (fread(buf, 1, hdr.payload_size, f) != hdr.payload_size) {
        heap_caps_free(buf);
        fclose(f);
        ESP_LOGE(TAG, "dialog id=%u: leitura do payload falhou", id);
        return ESP_ERR_INVALID_SIZE;
    }
    fclose(f);

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

    ESP_LOGI(TAG, "dialog id=%u carregado do SD: %u linhas, %u B payload",
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
