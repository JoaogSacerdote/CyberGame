#include "asset_store.h"

#include <string.h>

#include "esp_log.h"
#include "esp_rom_crc.h"

#include "storage_hal.h"

static const char *TAG = "ASSET_STORE";

/* ============================================================ Layout === */

#define ASSET_MAGIC_MANIFEST   0x46414D43u   /* 'CMAF' little-endian */
#define ASSET_MAGIC_ENTRY      0x53414743u   /* 'CGAS' little-endian */

#define MANIFEST_VERSION       1u

#define MANIFEST_BLOCK_START   1u
#define MANIFEST_NUM_COPIES    3u            /* blocos 1, 2, 3 */

#define CAT_SPRITE_BLOCK_START   4u
#define CAT_SPRITE_BLOCK_COUNT  96u
#define CAT_FONT_BLOCK_START   100u
#define CAT_FONT_BLOCK_COUNT    50u
#define CAT_SOUND_BLOCK_START  150u
#define CAT_SOUND_BLOCK_COUNT   50u

#define MAX_ENTRIES_PER_PAGE   31u           /* (2048 - 64) / 64 = 31 */

/* CRC do header cobre os campos: magic, version, num_entries, generation. */
#define HEADER_CRC_RANGE        12u

/* ============================================================ Tipos === */

typedef struct __attribute__((packed)) {
    uint32_t magic;            /* CGAS */
    uint8_t  type;
    uint8_t  reserved0;
    uint16_t id;
    uint16_t block_start;
    uint16_t block_count;
    uint32_t size_bytes;
    uint32_t crc32;
    char     name[ASSET_NAME_MAX];
    uint32_t reserved1;
} entry_on_disk_t;

_Static_assert(sizeof(entry_on_disk_t) == 64, "entry_on_disk_t must be 64 bytes");

typedef struct __attribute__((packed)) {
    uint32_t magic;            /* CMAF */
    uint16_t version;
    uint16_t num_entries;
    uint32_t generation;
    uint32_t header_crc;
    uint8_t  reserved[48];
} manifest_header_on_disk_t;

_Static_assert(sizeof(manifest_header_on_disk_t) == 64, "manifest header must be 64 bytes");

/* ============================================================ Estado === */

static bool                     s_inited     = false;
static uint32_t                 s_generation = 0;
static uint32_t                 s_active_block = UINT32_MAX;  /* qual copia esta ativa */
static uint16_t                 s_num_entries = 0;
static entry_on_disk_t          s_entries[MAX_ENTRIES_PER_PAGE];

/* Buffer estatico de leitura (2 KB). Usado em init e read. Funcoes que
 * chamam read NAO sao reentrantes. */
static uint8_t                  s_page_buf[STORAGE_PAGE_SIZE];

/* ===================================================== Helpers internos === */

static inline uint32_t header_crc(const manifest_header_on_disk_t *h)
{
    /* Cobre magic..generation = 12 bytes. */
    return esp_rom_crc32_le(0, (const uint8_t *)h, HEADER_CRC_RANGE);
}

static bool validate_header(const manifest_header_on_disk_t *h)
{
    if (h->magic != ASSET_MAGIC_MANIFEST) return false;
    if (h->version != MANIFEST_VERSION)   return false;
    if (h->num_entries > MAX_ENTRIES_PER_PAGE) return false;
    if (h->header_crc != header_crc(h))   return false;
    return true;
}

static bool validate_entry(const entry_on_disk_t *e)
{
    if (e->magic != ASSET_MAGIC_ENTRY)     return false;
    if (e->type  >= ASSET_TYPE_MAX)        return false;
    if (e->block_count == 0)               return false;
    if (e->size_bytes  == 0)               return false;
    /* Tamanho deve caber nos blocos alocados. */
    if (e->size_bytes > (uint32_t)e->block_count * STORAGE_BLOCK_SIZE) return false;
    return true;
}

static const entry_on_disk_t *find_entry(asset_type_t type, uint16_t id)
{
    for (uint16_t i = 0; i < s_num_entries; ++i) {
        const entry_on_disk_t *e = &s_entries[i];
        if (e->type == (uint8_t)type && e->id == id) {
            return e;
        }
    }
    return NULL;
}

/* ============================================================ Init === */

esp_err_t asset_store_init(void)
{
    if (s_inited) {
        return ESP_OK;
    }

    s_generation   = 0;
    s_active_block = UINT32_MAX;
    s_num_entries  = 0;
    memset(s_entries, 0, sizeof(s_entries));

    /* Tenta as 3 copias do manifest. Fica com a de maior generation que
     * passe a validacao. Se nenhuma valer, comeca vazio (cold start). */
    for (uint32_t copy = 0; copy < MANIFEST_NUM_COPIES; ++copy) {
        const uint32_t block = MANIFEST_BLOCK_START + copy;

        if (storage_hal_is_block_bad(block)) {
            ESP_LOGW(TAG, "manifest copy bloco %u marcado bad — pulando", (unsigned)block);
            continue;
        }

        const uint32_t page = block * STORAGE_PAGES_PER_BLOCK;
        if (storage_hal_read_page(page, s_page_buf) != ESP_OK) {
            ESP_LOGW(TAG, "falha lendo manifest copy bloco %u", (unsigned)block);
            continue;
        }

        const manifest_header_on_disk_t *h = (const manifest_header_on_disk_t *)s_page_buf;
        if (!validate_header(h)) {
            ESP_LOGD(TAG, "manifest copy bloco %u invalido (magic/version/crc)", (unsigned)block);
            continue;
        }

        if (h->generation <= s_generation && s_active_block != UINT32_MAX) {
            /* Ja temos uma copia mais nova. */
            continue;
        }

        /* Esta copia eh a melhor candidata. Copia entries para cache. */
        const uint8_t *entries_base = s_page_buf + sizeof(manifest_header_on_disk_t);
        memcpy(s_entries, entries_base, h->num_entries * sizeof(entry_on_disk_t));

        /* Drop entries que nao passem na validacao (corrompidas). */
        uint16_t valid = 0;
        for (uint16_t i = 0; i < h->num_entries; ++i) {
            if (validate_entry(&s_entries[i])) {
                if (valid != i) {
                    s_entries[valid] = s_entries[i];
                }
                ++valid;
            } else {
                ESP_LOGW(TAG, "entry %u no manifest gen %u eh invalida — descartada",
                         i, (unsigned)h->generation);
            }
        }
        memset(&s_entries[valid], 0, (MAX_ENTRIES_PER_PAGE - valid) * sizeof(entry_on_disk_t));

        s_num_entries  = valid;
        s_generation   = h->generation;
        s_active_block = block;
    }

    if (s_active_block == UINT32_MAX) {
        ESP_LOGI(TAG, "manifest nao encontrado — cold start (0 assets)");
    } else {
        ESP_LOGI(TAG, "manifest carregado do bloco %u (gen %u, %u entries)",
                 (unsigned)s_active_block, (unsigned)s_generation,
                 (unsigned)s_num_entries);
    }

    s_inited = true;
    return ESP_OK;
}

/* ============================================================ Lookup === */

esp_err_t asset_store_get_info(asset_type_t type, uint16_t id, asset_info_t *out)
{
    if (!s_inited)           return ESP_ERR_INVALID_STATE;
    if (!out)                return ESP_ERR_INVALID_ARG;
    if (type >= ASSET_TYPE_MAX) return ESP_ERR_INVALID_ARG;

    const entry_on_disk_t *e = find_entry(type, id);
    if (!e) return ESP_ERR_NOT_FOUND;

    out->type = (asset_type_t)e->type;
    out->id   = e->id;
    out->size = e->size_bytes;
    out->crc  = e->crc32;
    memcpy(out->name, e->name, ASSET_NAME_MAX);
    return ESP_OK;
}

/* ============================================================ Read === */

esp_err_t asset_store_read(asset_type_t type, uint16_t id,
                           size_t offset, void *buf, size_t size)
{
    if (!s_inited)           return ESP_ERR_INVALID_STATE;
    if (!buf && size > 0)    return ESP_ERR_INVALID_ARG;
    if (type >= ASSET_TYPE_MAX) return ESP_ERR_INVALID_ARG;

    const entry_on_disk_t *e = find_entry(type, id);
    if (!e) return ESP_ERR_NOT_FOUND;

    if (offset > e->size_bytes || offset + size > e->size_bytes) {
        return ESP_ERR_INVALID_ARG;
    }
    if (size == 0) return ESP_OK;

    /* Endereco absoluto em bytes dentro da NAND. */
    const uint64_t start_byte = (uint64_t)e->block_start * STORAGE_BLOCK_SIZE + offset;
    uint64_t       cur_byte   = start_byte;
    const uint64_t end_byte   = start_byte + size;
    uint8_t       *dst        = (uint8_t *)buf;

    while (cur_byte < end_byte) {
        const uint32_t page         = (uint32_t)(cur_byte / STORAGE_PAGE_SIZE);
        const uint32_t off_in_page  = (uint32_t)(cur_byte % STORAGE_PAGE_SIZE);
        const uint32_t chunk        = (uint32_t)((end_byte - cur_byte) > (STORAGE_PAGE_SIZE - off_in_page)
                                                ? (STORAGE_PAGE_SIZE - off_in_page)
                                                : (end_byte - cur_byte));

        esp_err_t err = storage_hal_read_page(page, s_page_buf);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "read_page(%u) falhou: %s", (unsigned)page,
                     esp_err_to_name(err));
            return err;
        }
        memcpy(dst, s_page_buf + off_in_page, chunk);
        dst      += chunk;
        cur_byte += chunk;
    }

    return ESP_OK;
}

/* ============================================================ List === */

esp_err_t asset_store_count(size_t *out_count)
{
    if (!s_inited) return ESP_ERR_INVALID_STATE;
    if (!out_count) return ESP_ERR_INVALID_ARG;
    *out_count = s_num_entries;
    return ESP_OK;
}

esp_err_t asset_store_list(asset_info_t *out, size_t max, size_t *actual)
{
    if (!s_inited)           return ESP_ERR_INVALID_STATE;
    if (!out && max > 0)     return ESP_ERR_INVALID_ARG;
    if (!actual)             return ESP_ERR_INVALID_ARG;

    const size_t n = (s_num_entries < max) ? s_num_entries : max;
    for (size_t i = 0; i < n; ++i) {
        out[i].type = (asset_type_t)s_entries[i].type;
        out[i].id   = s_entries[i].id;
        out[i].size = s_entries[i].size_bytes;
        out[i].crc  = s_entries[i].crc32;
        memcpy(out[i].name, s_entries[i].name, ASSET_NAME_MAX);
    }
    *actual = n;
    return ESP_OK;
}
