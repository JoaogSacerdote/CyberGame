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

/* Buffer estatico de leitura (2 KB). Usado em init, read e write_manifest.
 * Operacoes NAO sao reentrantes — uma flag atomica detecta violacao desse
 * contrato (varias tasks usando o asset_store concorrentemente) e loga
 * ERROR sem corromper o estado. */
static uint8_t                  s_page_buf[STORAGE_PAGE_SIZE];
static volatile bool            s_page_buf_busy;

static inline bool page_buf_try_acquire(const char *fn)
{
    if (__atomic_exchange_n(&s_page_buf_busy, true, __ATOMIC_ACQUIRE)) {
        ESP_LOGE(TAG, "REENTRANCIA em %s — s_page_buf ja em uso", fn);
        return false;
    }
    return true;
}
static inline void page_buf_release(void)
{
    __atomic_store_n(&s_page_buf_busy, false, __ATOMIC_RELEASE);
}

/* Sessao de escrita (Etapa 2). Apenas UMA por vez. */
typedef struct {
    bool         active;
    uint8_t      type;
    uint16_t     id;
    char         name[ASSET_NAME_MAX];
    uint16_t     block_start;       /* primeiro bloco alocado */
    uint16_t     blocks_alloced;    /* quantos blocos foram apagados */
    uint32_t     expected_size;     /* total bytes esperados */
    uint32_t     bytes_written;     /* bytes ja consumidos por write_chunk */
    uint32_t     crc_accum;         /* CRC32 acumulado sobre o payload util */
    uint32_t     next_page;         /* proxima pagina a escrever */
    uint16_t     page_buf_used;     /* bytes acumulados no s_write_page_buf */
} write_session_t;

static write_session_t s_write = { 0 };
static uint8_t         s_write_page_buf[STORAGE_PAGE_SIZE];

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

    if (!page_buf_try_acquire("asset_store_read")) return ESP_ERR_INVALID_STATE;

    /* Endereco absoluto em bytes dentro da NAND. */
    const uint64_t start_byte = (uint64_t)e->block_start * STORAGE_BLOCK_SIZE + offset;
    uint64_t       cur_byte   = start_byte;
    const uint64_t end_byte   = start_byte + size;
    uint8_t       *dst        = (uint8_t *)buf;
    esp_err_t      err        = ESP_OK;

    while (cur_byte < end_byte) {
        const uint32_t page         = (uint32_t)(cur_byte / STORAGE_PAGE_SIZE);
        const uint32_t off_in_page  = (uint32_t)(cur_byte % STORAGE_PAGE_SIZE);
        const uint32_t chunk        = (uint32_t)((end_byte - cur_byte) > (STORAGE_PAGE_SIZE - off_in_page)
                                                ? (STORAGE_PAGE_SIZE - off_in_page)
                                                : (end_byte - cur_byte));

        err = storage_hal_read_page(page, s_page_buf);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "read_page(%u) falhou: %s", (unsigned)page,
                     esp_err_to_name(err));
            break;
        }
        memcpy(dst, s_page_buf + off_in_page, chunk);
        dst      += chunk;
        cur_byte += chunk;
    }

    page_buf_release();
    return err;
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

/* ============================================================ Write === */

static void cat_range(asset_type_t type, uint16_t *start, uint16_t *count)
{
    switch (type) {
        case ASSET_TYPE_SPRITE: *start = CAT_SPRITE_BLOCK_START; *count = CAT_SPRITE_BLOCK_COUNT; break;
        case ASSET_TYPE_FONT:   *start = CAT_FONT_BLOCK_START;   *count = CAT_FONT_BLOCK_COUNT;   break;
        case ASSET_TYPE_SOUND:  *start = CAT_SOUND_BLOCK_START;  *count = CAT_SOUND_BLOCK_COUNT;  break;
        default:                *start = 0; *count = 0;
    }
}

/* Alocador bump: aplica imediatamente apos o ultimo asset da categoria. */
static esp_err_t alloc_blocks(asset_type_t type, uint16_t blocks_needed,
                              uint16_t *out_block_start)
{
    uint16_t cat_start, cat_count;
    cat_range(type, &cat_start, &cat_count);
    if (blocks_needed == 0 || blocks_needed > cat_count) return ESP_ERR_INVALID_ARG;

    uint16_t bump = cat_start;
    for (uint16_t i = 0; i < s_num_entries; ++i) {
        const entry_on_disk_t *e = &s_entries[i];
        if (e->type == (uint8_t)type) {
            uint16_t end = e->block_start + e->block_count;
            if (end > bump) bump = end;
        }
    }

    /* Busca janela de blocks_needed blocos consecutivos sem bad block. */
    while (bump + blocks_needed <= cat_start + cat_count) {
        bool ok = true;
        uint16_t bad_at = 0;
        for (uint16_t i = 0; i < blocks_needed; ++i) {
            if (storage_hal_is_block_bad(bump + i)) {
                ok = false;
                bad_at = i;
                break;
            }
        }
        if (ok) {
            *out_block_start = bump;
            return ESP_OK;
        }
        bump += bad_at + 1;
    }
    ESP_LOGE(TAG, "categoria %d sem espaco contiguo para %u blocos",
             (int)type, (unsigned)blocks_needed);
    return ESP_ERR_NO_MEM;
}

static uint32_t next_manifest_block(void)
{
    if (s_active_block == UINT32_MAX) return MANIFEST_BLOCK_START;
    uint32_t next = s_active_block + 1;
    if (next >= MANIFEST_BLOCK_START + MANIFEST_NUM_COPIES) {
        next = MANIFEST_BLOCK_START;
    }
    return next;
}

/* Grava o array de entries (cache em RAM) no proximo bloco do manifest,
 * bumping generation. Atualiza s_active_block / s_generation. */
static esp_err_t write_manifest(void)
{
    const uint32_t target_block = next_manifest_block();

    if (storage_hal_is_block_bad(target_block)) {
        ESP_LOGE(TAG, "manifest target bloco %u marcado bad", (unsigned)target_block);
        return ESP_ERR_INVALID_STATE;
    }
    esp_err_t err = storage_hal_erase_block(target_block);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "erase_block(%u) falhou: %s", (unsigned)target_block,
                 esp_err_to_name(err));
        return err;
    }

    /* Monta pagina 0: header + entries. */
    memset(s_page_buf, 0xFF, STORAGE_PAGE_SIZE);
    manifest_header_on_disk_t *h = (manifest_header_on_disk_t *)s_page_buf;
    h->magic       = ASSET_MAGIC_MANIFEST;
    h->version     = MANIFEST_VERSION;
    h->num_entries = s_num_entries;
    h->generation  = s_generation + 1;
    h->header_crc  = header_crc(h);

    memcpy(s_page_buf + sizeof(manifest_header_on_disk_t),
           s_entries, s_num_entries * sizeof(entry_on_disk_t));

    err = storage_hal_write_page(target_block * STORAGE_PAGES_PER_BLOCK, s_page_buf);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "write_page manifest falhou: %s", esp_err_to_name(err));
        return err;
    }

    s_active_block = target_block;
    s_generation  += 1;
    ESP_LOGI(TAG, "manifest gravado no bloco %u (gen %u, %u entries)",
             (unsigned)s_active_block, (unsigned)s_generation,
             (unsigned)s_num_entries);
    return ESP_OK;
}

esp_err_t asset_store_begin_write(asset_type_t type, uint16_t id,
                                  const char *name, uint32_t expected_size,
                                  asset_write_handle_t *out_handle)
{
    if (!s_inited)              return ESP_ERR_INVALID_STATE;
    if (s_write.active)         return ESP_ERR_INVALID_STATE;
    if (type >= ASSET_TYPE_MAX) return ESP_ERR_INVALID_ARG;
    if (!name || expected_size == 0 || !out_handle) return ESP_ERR_INVALID_ARG;
    if (s_num_entries >= MAX_ENTRIES_PER_PAGE) return ESP_ERR_NO_MEM;

    const uint16_t blocks_needed = (uint16_t)((expected_size + STORAGE_BLOCK_SIZE - 1) / STORAGE_BLOCK_SIZE);

    uint16_t block_start = 0;
    esp_err_t err = alloc_blocks(type, blocks_needed, &block_start);
    if (err != ESP_OK) return err;

    /* Apaga todos os blocos da janela. */
    for (uint16_t i = 0; i < blocks_needed; ++i) {
        err = storage_hal_erase_block(block_start + i);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "erase_block(%u) falhou: %s",
                     (unsigned)(block_start + i), esp_err_to_name(err));
            return err;
        }
    }

    memset(&s_write, 0, sizeof(s_write));
    s_write.active         = true;
    s_write.type           = (uint8_t)type;
    s_write.id             = id;
    s_write.block_start    = block_start;
    s_write.blocks_alloced = blocks_needed;
    s_write.expected_size  = expected_size;
    s_write.next_page      = block_start * STORAGE_PAGES_PER_BLOCK;
    s_write.crc_accum      = 0;
    s_write.page_buf_used  = 0;
    strncpy(s_write.name, name, ASSET_NAME_MAX - 1);
    s_write.name[ASSET_NAME_MAX - 1] = '\0';
    memset(s_write_page_buf, 0xFF, STORAGE_PAGE_SIZE);

    *out_handle = (asset_write_handle_t)&s_write;
    ESP_LOGI(TAG, "begin_write type=%d id=%u name='%s' size=%u blocos=%u start=%u",
             (int)type, id, s_write.name, (unsigned)expected_size,
             (unsigned)blocks_needed, (unsigned)block_start);
    return ESP_OK;
}

static esp_err_t flush_page(void)
{
    /* Preenche resto com 0xFF e escreve. */
    if (s_write.page_buf_used == 0) return ESP_OK;
    if (s_write.page_buf_used < STORAGE_PAGE_SIZE) {
        memset(s_write_page_buf + s_write.page_buf_used, 0xFF,
               STORAGE_PAGE_SIZE - s_write.page_buf_used);
    }
    esp_err_t err = storage_hal_write_page(s_write.next_page, s_write_page_buf);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "write_page(%u) falhou: %s",
                 (unsigned)s_write.next_page, esp_err_to_name(err));
        return err;
    }
    s_write.next_page    += 1;
    s_write.page_buf_used = 0;
    memset(s_write_page_buf, 0xFF, STORAGE_PAGE_SIZE);
    return ESP_OK;
}

esp_err_t asset_store_write_chunk(asset_write_handle_t handle,
                                  const void *data, size_t len)
{
    if (handle != (asset_write_handle_t)&s_write || !s_write.active) {
        return ESP_ERR_INVALID_STATE;
    }
    if (!data && len > 0) return ESP_ERR_INVALID_ARG;
    if (len == 0) return ESP_OK;
    if (s_write.bytes_written + len > s_write.expected_size) {
        ESP_LOGE(TAG, "write_chunk excede expected_size (%u + %u > %u)",
                 (unsigned)s_write.bytes_written, (unsigned)len,
                 (unsigned)s_write.expected_size);
        return ESP_ERR_INVALID_ARG;
    }

    const uint8_t *src = (const uint8_t *)data;
    size_t remaining = len;
    while (remaining > 0) {
        const size_t cap = STORAGE_PAGE_SIZE - s_write.page_buf_used;
        const size_t take = remaining < cap ? remaining : cap;
        memcpy(s_write_page_buf + s_write.page_buf_used, src, take);
        s_write.page_buf_used += take;
        src                   += take;
        remaining             -= take;

        if (s_write.page_buf_used == STORAGE_PAGE_SIZE) {
            esp_err_t err = flush_page();
            if (err != ESP_OK) return err;
        }
    }

    s_write.crc_accum = esp_rom_crc32_le(s_write.crc_accum, (const uint8_t *)data, len);
    s_write.bytes_written += len;
    return ESP_OK;
}

/* Remove entry existente (mesmo type+id) do array de entries em memoria. */
static void remove_existing_entry(uint8_t type, uint16_t id)
{
    for (uint16_t i = 0; i < s_num_entries; ++i) {
        if (s_entries[i].type == type && s_entries[i].id == id) {
            for (uint16_t j = i + 1; j < s_num_entries; ++j) {
                s_entries[j - 1] = s_entries[j];
            }
            --s_num_entries;
            memset(&s_entries[s_num_entries], 0, sizeof(entry_on_disk_t));
            return;
        }
    }
}

esp_err_t asset_store_commit_write(asset_write_handle_t handle)
{
    if (handle != (asset_write_handle_t)&s_write || !s_write.active) {
        return ESP_ERR_INVALID_STATE;
    }
    if (s_write.bytes_written != s_write.expected_size) {
        ESP_LOGE(TAG, "commit_write incompleto: %u/%u bytes",
                 (unsigned)s_write.bytes_written, (unsigned)s_write.expected_size);
        return ESP_ERR_INVALID_STATE;
    }

    esp_err_t err = flush_page();
    if (err != ESP_OK) return err;

    /* Atualiza array de entries (overwrite se ja existir). */
    remove_existing_entry(s_write.type, s_write.id);
    if (s_num_entries >= MAX_ENTRIES_PER_PAGE) {
        s_write.active = false;
        return ESP_ERR_NO_MEM;
    }
    entry_on_disk_t *e = &s_entries[s_num_entries++];
    memset(e, 0, sizeof(*e));
    e->magic       = ASSET_MAGIC_ENTRY;
    e->type        = s_write.type;
    e->id          = s_write.id;
    e->block_start = s_write.block_start;
    e->block_count = s_write.blocks_alloced;
    e->size_bytes  = s_write.expected_size;
    e->crc32       = s_write.crc_accum;
    strncpy(e->name, s_write.name, ASSET_NAME_MAX - 1);

    /* Grava manifest. Se falhar, mantem o cache mas a NAND fica inconsistente —
     * pos-reboot, o init recupera o manifest anterior (de menor generation). */
    err = write_manifest();
    s_write.active = false;
    return err;
}

esp_err_t asset_store_abort_write(asset_write_handle_t handle)
{
    if (handle != (asset_write_handle_t)&s_write || !s_write.active) {
        return ESP_ERR_INVALID_STATE;
    }
    ESP_LOGW(TAG, "abort_write — %u bytes desperdicados em %u blocos",
             (unsigned)s_write.bytes_written, (unsigned)s_write.blocks_alloced);
    s_write.active = false;
    return ESP_OK;
}

esp_err_t asset_store_erase_category(asset_type_t type)
{
    if (!s_inited)              return ESP_ERR_INVALID_STATE;
    if (s_write.active)         return ESP_ERR_INVALID_STATE;
    if (type >= ASSET_TYPE_MAX) return ESP_ERR_INVALID_ARG;

    /* Erase fisico dos blocos dos assets desta categoria. */
    uint16_t kept = 0;
    for (uint16_t i = 0; i < s_num_entries; ++i) {
        entry_on_disk_t *e = &s_entries[i];
        if (e->type == (uint8_t)type) {
            for (uint16_t b = 0; b < e->block_count; ++b) {
                if (!storage_hal_is_block_bad(e->block_start + b)) {
                    storage_hal_erase_block(e->block_start + b);
                }
            }
        } else {
            if (kept != i) s_entries[kept] = *e;
            ++kept;
        }
    }
    memset(&s_entries[kept], 0, (s_num_entries - kept) * sizeof(entry_on_disk_t));
    s_num_entries = kept;
    ESP_LOGI(TAG, "erase_category type=%d -> %u entries restantes", (int)type, kept);
    return write_manifest();
}

esp_err_t asset_store_factory_reset(void)
{
    if (!s_inited)      return ESP_ERR_INVALID_STATE;
    if (s_write.active) return ESP_ERR_INVALID_STATE;

    /* Apaga blocos de manifest (1..3). Erase tolera bad, so loga. */
    for (uint32_t b = MANIFEST_BLOCK_START;
         b < MANIFEST_BLOCK_START + MANIFEST_NUM_COPIES; ++b) {
        if (!storage_hal_is_block_bad(b)) {
            storage_hal_erase_block(b);
        }
    }
    /* Apaga blocos dos assets atuais. */
    for (uint16_t i = 0; i < s_num_entries; ++i) {
        entry_on_disk_t *e = &s_entries[i];
        for (uint16_t b = 0; b < e->block_count; ++b) {
            if (!storage_hal_is_block_bad(e->block_start + b)) {
                storage_hal_erase_block(e->block_start + b);
            }
        }
    }

    memset(s_entries, 0, sizeof(s_entries));
    s_num_entries  = 0;
    s_generation   = 0;
    s_active_block = UINT32_MAX;
    ESP_LOGW(TAG, "factory_reset concluido — manifest e assets apagados");
    return ESP_OK;
}
