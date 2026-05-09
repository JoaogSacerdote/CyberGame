#include "storage_hal.h"

#include <string.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/spi_master.h"
#include "esp_check.h"
#include "esp_heap_caps.h"
#include "esp_log.h"

static const char *TAG = "STORAGE_HAL";

#define STORAGE_PIN_MOSI       7
#define STORAGE_PIN_SCK        8
#define STORAGE_PIN_MISO       9
#define STORAGE_PIN_CS         10

#define STORAGE_SPI_HOST       SPI2_HOST
/* 10 MHz conservador para bring-up em protoboard. NAND aguenta 104 MHz, mas
 * fios longos / sem decoupling sao sensiveis. Quando estabilizar, subir. */
#define STORAGE_SPI_HZ         (10 * 1000 * 1000)

/* W25N01GV NAND commands */
#define NAND_CMD_RESET           0xFF
#define NAND_CMD_JEDEC_ID        0x9F
#define NAND_CMD_GET_FEATURE     0x0F
#define NAND_CMD_SET_FEATURE     0x1F
#define NAND_CMD_WRITE_ENABLE    0x06
#define NAND_CMD_BLOCK_ERASE     0xD8
#define NAND_CMD_PROGRAM_LOAD    0x02
#define NAND_CMD_PROGRAM_EXEC    0x10
#define NAND_CMD_PAGE_READ       0x13
#define NAND_CMD_READ_FROM_CACHE 0x03

/* Feature register addresses */
#define NAND_REG_PROTECTION      0xA0
#define NAND_REG_CONFIG          0xB0
#define NAND_REG_STATUS          0xC0

/* Status register bits (reg 0xC0) */
#define NAND_STATUS_OIP          (1 << 0)
#define NAND_STATUS_WEL          (1 << 1)
#define NAND_STATUS_E_FAIL       (1 << 2)
#define NAND_STATUS_P_FAIL       (1 << 3)

/* Configuration register bits (reg 0xB0) */
#define NAND_CONFIG_BUF          (1 << 3)
#define NAND_CONFIG_ECC_E        (1 << 4)

/* JEDEC ID esperado: Winbond W25N01GV = EF AA 21 */
#define NAND_EXPECTED_MANUF      0xEF
#define NAND_EXPECTED_DEV_HI     0xAA
#define NAND_EXPECTED_DEV_LO     0x21

/* Bloco reservado para POST destrutivo — ultimo bloco do chip, fora da area
 * de assets futuros do jogo. */
#define NAND_TEST_BLOCK          1023u

static spi_device_handle_t s_spi = NULL;

/* Buffer de transferencia para read/write de pagina inteira. ~2 KB em BSS.
 * Nao reentrante — storage_hal nao eh chamado por multiplas tarefas. */
static uint8_t s_xfer_buf[STORAGE_PAGE_SIZE + 4];

static esp_err_t storage_spi_init(void)
{
    const spi_bus_config_t bus_cfg = {
        .mosi_io_num     = STORAGE_PIN_MOSI,
        .miso_io_num     = STORAGE_PIN_MISO,
        .sclk_io_num     = STORAGE_PIN_SCK,
        .quadwp_io_num   = -1,
        .quadhd_io_num   = -1,
        .max_transfer_sz = 4096,
    };

    /* Bus pode ja ter sido inicializado pelo display_hal — tolerar. */
    const esp_err_t ret = spi_bus_initialize(STORAGE_SPI_HOST, &bus_cfg, SPI_DMA_CH_AUTO);
    if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
        return ret;
    }

    const spi_device_interface_config_t dev_cfg = {
        .clock_speed_hz = STORAGE_SPI_HZ,
        .mode           = 0,
        .spics_io_num   = STORAGE_PIN_CS,
        .queue_size     = 4,
        .command_bits   = 8,
        .address_bits   = 0,
        .dummy_bits     = 0,
    };
    return spi_bus_add_device(STORAGE_SPI_HOST, &dev_cfg, &s_spi);
}

/* --- Helpers internos --- */

static esp_err_t nand_cmd_only(uint8_t cmd)
{
    spi_transaction_t t = {
        .cmd    = cmd,
        .length = 0,
    };
    return spi_device_polling_transmit(s_spi, &t);
}

/* Get Feature: cmd(0x0F) + reg + read 1 byte. */
static esp_err_t nand_get_feature(uint8_t reg, uint8_t *val)
{
    uint8_t txrx[2] = { reg, 0xFF };
    spi_transaction_t t = {
        .cmd       = NAND_CMD_GET_FEATURE,
        .length    = 16,
        .tx_buffer = txrx,
        .rx_buffer = txrx,
    };
    const esp_err_t err = spi_device_polling_transmit(s_spi, &t);
    if (err == ESP_OK) *val = txrx[1];
    return err;
}

/* Set Feature: cmd(0x1F) + reg + write 1 byte. */
static esp_err_t nand_set_feature(uint8_t reg, uint8_t val)
{
    uint8_t tx[2] = { reg, val };
    spi_transaction_t t = {
        .cmd       = NAND_CMD_SET_FEATURE,
        .length    = 16,
        .tx_buffer = tx,
    };
    return spi_device_polling_transmit(s_spi, &t);
}

/* Polla bit OIP do status register ate clear ou timeout.
 * Operacoes do NAND sao curtas (us a poucos ms) — tight loop eh OK. */
static esp_err_t nand_wait_oip(uint32_t timeout_ms)
{
    const TickType_t deadline = xTaskGetTickCount() + pdMS_TO_TICKS(timeout_ms);
    uint8_t status = 0;
    do {
        ESP_RETURN_ON_ERROR(nand_get_feature(NAND_REG_STATUS, &status),
                            TAG, "get status failed");
        if ((status & NAND_STATUS_OIP) == 0) return ESP_OK;
    } while (xTaskGetTickCount() < deadline);
    return ESP_ERR_TIMEOUT;
}

static esp_err_t nand_write_enable(void)
{
    return nand_cmd_only(NAND_CMD_WRITE_ENABLE);
}

/* --- API publica --- */

esp_err_t storage_hal_read_jedec_id(uint8_t *manuf, uint16_t *device)
{
    if (s_spi == NULL) return ESP_ERR_INVALID_STATE;
    if (manuf == NULL || device == NULL) return ESP_ERR_INVALID_ARG;

    /* Comando 0x9F — sequencia exata varia entre variantes do W25N:
     *   sem dummy: cmd + 3 ID bytes (manuf + dev_hi + dev_lo)
     *   com dummy: cmd + 1 dummy + 3 ID bytes
     * Lemos 4 bytes apos o comando e procuramos a assinatura 0xEF em ambos
     * os offsets — robusto a qualquer um dos dois layouts. */
    uint8_t rx[4] = { 0 };
    spi_transaction_t t = {
        .cmd       = NAND_CMD_JEDEC_ID,
        .length    = 32,
        .rxlength  = 32,
        .rx_buffer = rx,
    };
    ESP_RETURN_ON_ERROR(spi_device_polling_transmit(s_spi, &t), TAG, "JEDEC ID read failed");

    ESP_LOGI(TAG, "Raw JEDEC bytes: %02X %02X %02X %02X", rx[0], rx[1], rx[2], rx[3]);

    if (rx[0] == NAND_EXPECTED_MANUF) {
        *manuf  = rx[0];
        *device = ((uint16_t)rx[1] << 8) | rx[2];
        return ESP_OK;
    }
    if (rx[1] == NAND_EXPECTED_MANUF) {
        *manuf  = rx[1];
        *device = ((uint16_t)rx[2] << 8) | rx[3];
        return ESP_OK;
    }
    return ESP_ERR_NOT_FOUND;
}

esp_err_t storage_hal_init(void)
{
    if (s_spi != NULL) {
        return ESP_OK;
    }

    ESP_RETURN_ON_ERROR(storage_spi_init(), TAG, "SPI init failed");

    /* Reset opcional — algumas NANDs ficam em estado indefinido apos power-up.
     * Se falhar (chip nao responde), seguimos para o JEDEC ID que e o teste real. */
    spi_transaction_t reset_t = {
        .cmd    = NAND_CMD_RESET,
        .length = 0,
    };
    if (spi_device_polling_transmit(s_spi, &reset_t) != ESP_OK) {
        ESP_LOGW(TAG, "Reset command falhou (continuando)");
    }
    vTaskDelay(pdMS_TO_TICKS(2));    /* tRST max 1.5ms */

    uint8_t  manuf  = 0;
    uint16_t device = 0;
    const esp_err_t ret = storage_hal_read_jedec_id(&manuf, &device);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "JEDEC ID nao reconhecido (chip nao respondeu ou modelo diferente).");
        ESP_LOGE(TAG, "Verificar fios: CS=GPIO10, MISO=GPIO9, MOSI=GPIO7, SCK=GPIO8, /WP e /HOLD em 3.3V");
        return ret;
    }

    if (manuf == NAND_EXPECTED_MANUF &&
        device == ((uint16_t)NAND_EXPECTED_DEV_HI << 8 | NAND_EXPECTED_DEV_LO)) {
        ESP_LOGI(TAG, "NAND detectada: Winbond W25N01GV (1Gbit / 128MB) — JEDEC %02X %04X",
                 manuf, device);
    } else {
        ESP_LOGW(TAG, "Chip respondeu mas nao eh W25N01GV — JEDEC %02X %04X (esperado EF AA21)",
                 manuf, device);
    }

    /* Tira protecao de escrita default (bits BP do Protection Register).
     * Sem isso, qualquer programa/erase falha silenciosamente. */
    if (nand_set_feature(NAND_REG_PROTECTION, 0x00) != ESP_OK) {
        ESP_LOGW(TAG, "Nao consegui limpar Protection Register — escritas podem falhar");
    }

    /* Garante BUF=1 (cache buffer mode) e ECC-E=1 (correcao de erros automatica). */
    if (nand_set_feature(NAND_REG_CONFIG, NAND_CONFIG_BUF | NAND_CONFIG_ECC_E) != ESP_OK) {
        ESP_LOGW(TAG, "Nao consegui setar Config Register — operando com defaults");
    }

    return ESP_OK;
}

esp_err_t storage_hal_read_page(uint32_t page, uint8_t *buf)
{
    if (s_spi == NULL) return ESP_ERR_INVALID_STATE;
    if (buf == NULL || page >= STORAGE_TOTAL_PAGES) return ESP_ERR_INVALID_ARG;

    /* Step 1: Page Data Read — array NAND -> cache interna do chip.
     * Formato: cmd(0x13) + 1 dummy + 16-bit page address. */
    uint8_t prefix[3] = { 0x00, (uint8_t)(page >> 8), (uint8_t)page };
    spi_transaction_t t1 = {
        .cmd       = NAND_CMD_PAGE_READ,
        .length    = 24,
        .tx_buffer = prefix,
    };
    ESP_RETURN_ON_ERROR(spi_device_polling_transmit(s_spi, &t1), TAG, "page read cmd failed");
    ESP_RETURN_ON_ERROR(nand_wait_oip(50), TAG, "page read OIP timeout");

    /* Step 2: Read From Cache — cache do chip -> host.
     * Formato: cmd(0x03) + 16-bit column + 8 dummy + N bytes data. */
    memset(s_xfer_buf, 0, 3 + STORAGE_PAGE_SIZE);
    spi_transaction_t t2 = {
        .cmd       = NAND_CMD_READ_FROM_CACHE,
        .length    = (3 + STORAGE_PAGE_SIZE) * 8,
        .tx_buffer = s_xfer_buf,
        .rx_buffer = s_xfer_buf,
    };
    ESP_RETURN_ON_ERROR(spi_device_polling_transmit(s_spi, &t2), TAG, "read from cache failed");

    memcpy(buf, &s_xfer_buf[3], STORAGE_PAGE_SIZE);
    return ESP_OK;
}

esp_err_t storage_hal_write_page(uint32_t page, const uint8_t *buf)
{
    if (s_spi == NULL) return ESP_ERR_INVALID_STATE;
    if (buf == NULL || page >= STORAGE_TOTAL_PAGES) return ESP_ERR_INVALID_ARG;

    ESP_RETURN_ON_ERROR(nand_write_enable(), TAG, "write enable failed");

    /* Step 1: Load Program Data — host -> cache interna do chip.
     * Formato: cmd(0x02) + 16-bit column + N bytes data. */
    s_xfer_buf[0] = 0x00;
    s_xfer_buf[1] = 0x00;
    memcpy(&s_xfer_buf[2], buf, STORAGE_PAGE_SIZE);
    spi_transaction_t t1 = {
        .cmd       = NAND_CMD_PROGRAM_LOAD,
        .length    = (2 + STORAGE_PAGE_SIZE) * 8,
        .tx_buffer = s_xfer_buf,
    };
    ESP_RETURN_ON_ERROR(spi_device_polling_transmit(s_spi, &t1), TAG, "program load failed");

    /* Step 2: Program Execute — commita cache para o array NAND.
     * Formato: cmd(0x10) + 1 dummy + 16-bit page address. */
    uint8_t exec[3] = { 0x00, (uint8_t)(page >> 8), (uint8_t)page };
    spi_transaction_t t2 = {
        .cmd       = NAND_CMD_PROGRAM_EXEC,
        .length    = 24,
        .tx_buffer = exec,
    };
    ESP_RETURN_ON_ERROR(spi_device_polling_transmit(s_spi, &t2), TAG, "program exec failed");

    ESP_RETURN_ON_ERROR(nand_wait_oip(10), TAG, "program OIP timeout");

    uint8_t status = 0;
    ESP_RETURN_ON_ERROR(nand_get_feature(NAND_REG_STATUS, &status), TAG, "status read failed");
    if (status & NAND_STATUS_P_FAIL) {
        ESP_LOGE(TAG, "Program FAIL na pagina %u (status=0x%02X)", (unsigned)page, status);
        return ESP_FAIL;
    }
    return ESP_OK;
}

esp_err_t storage_hal_erase_block(uint32_t block)
{
    if (s_spi == NULL) return ESP_ERR_INVALID_STATE;
    if (block >= STORAGE_BLOCK_COUNT) return ESP_ERR_INVALID_ARG;

    ESP_RETURN_ON_ERROR(nand_write_enable(), TAG, "write enable failed");

    /* Block Erase: cmd(0xD8) + 1 dummy + 16-bit page address (qualquer pagina do bloco). */
    const uint32_t page = block * STORAGE_PAGES_PER_BLOCK;
    uint8_t cmd[3] = { 0x00, (uint8_t)(page >> 8), (uint8_t)page };
    spi_transaction_t t = {
        .cmd       = NAND_CMD_BLOCK_ERASE,
        .length    = 24,
        .tx_buffer = cmd,
    };
    ESP_RETURN_ON_ERROR(spi_device_polling_transmit(s_spi, &t), TAG, "erase cmd failed");

    /* Erase tipico 4 ms, max 10 ms. Folga de 50 ms. */
    ESP_RETURN_ON_ERROR(nand_wait_oip(50), TAG, "erase OIP timeout");

    uint8_t status = 0;
    ESP_RETURN_ON_ERROR(nand_get_feature(NAND_REG_STATUS, &status), TAG, "status read failed");
    if (status & NAND_STATUS_E_FAIL) {
        ESP_LOGE(TAG, "Erase FAIL no bloco %u (status=0x%02X)", (unsigned)block, status);
        return ESP_FAIL;
    }
    return ESP_OK;
}

bool storage_hal_is_block_bad(uint32_t block)
{
    if (s_spi == NULL || block >= STORAGE_BLOCK_COUNT) return true;

    /* Bad block marker: byte 0 da spare area da pagina 0 do bloco.
     * 0xFF = bom; qualquer outro valor = ruim. */
    const uint32_t page = block * STORAGE_PAGES_PER_BLOCK;

    uint8_t prefix[3] = { 0x00, (uint8_t)(page >> 8), (uint8_t)page };
    spi_transaction_t t1 = {
        .cmd = NAND_CMD_PAGE_READ, .length = 24, .tx_buffer = prefix,
    };
    if (spi_device_polling_transmit(s_spi, &t1) != ESP_OK) return true;
    if (nand_wait_oip(50) != ESP_OK) return true;

    /* Read From Cache: column = 2048 (inicio da spare area), 1 byte util. */
    uint8_t buf[4] = { (uint8_t)(STORAGE_PAGE_SIZE >> 8),
                       (uint8_t)STORAGE_PAGE_SIZE, 0x00, 0xAA };
    spi_transaction_t t2 = {
        .cmd = NAND_CMD_READ_FROM_CACHE, .length = 32,
        .tx_buffer = buf, .rx_buffer = buf,
    };
    if (spi_device_polling_transmit(s_spi, &t2) != ESP_OK) return true;

    return buf[3] != 0xFF;
}

esp_err_t storage_hal_test_write_cycle(void)
{
    if (s_spi == NULL) return ESP_ERR_INVALID_STATE;

    ESP_LOGI(TAG, "POST: teste destrutivo no bloco %u (reservado)...", NAND_TEST_BLOCK);

    if (storage_hal_is_block_bad(NAND_TEST_BLOCK)) {
        ESP_LOGE(TAG, "Bloco %u marcado como bad block — abortando POST", NAND_TEST_BLOCK);
        return ESP_FAIL;
    }

    ESP_RETURN_ON_ERROR(storage_hal_erase_block(NAND_TEST_BLOCK), TAG, "erase failed");
    ESP_LOGI(TAG, "  Erase OK");

    uint8_t *pattern  = heap_caps_malloc(STORAGE_PAGE_SIZE, MALLOC_CAP_DMA);
    uint8_t *readback = heap_caps_malloc(STORAGE_PAGE_SIZE, MALLOC_CAP_DMA);
    if (pattern == NULL || readback == NULL) {
        free(pattern); free(readback);
        ESP_LOGE(TAG, "Falha ao alocar buffers de teste");
        return ESP_ERR_NO_MEM;
    }

    /* Verifica que erase realmente apagou (deve ser tudo 0xFF). */
    esp_err_t ret = storage_hal_read_page(NAND_TEST_BLOCK * STORAGE_PAGES_PER_BLOCK, readback);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Read pos-erase falhou");
        goto cleanup;
    }
    for (size_t i = 0; i < STORAGE_PAGE_SIZE; ++i) {
        if (readback[i] != 0xFF) {
            ESP_LOGE(TAG, "  Pos-erase FAIL: offset %u = 0x%02X (esperado 0xFF)",
                     (unsigned)i, readback[i]);
            ret = ESP_FAIL;
            goto cleanup;
        }
    }
    ESP_LOGI(TAG, "  Pos-erase: todos os %u bytes = 0xFF (OK)", STORAGE_PAGE_SIZE);

    /* Escreve padrao variado e relê. */
    for (size_t i = 0; i < STORAGE_PAGE_SIZE; ++i) {
        pattern[i] = (uint8_t)(0xA5 ^ (i & 0xFF));
    }

    const uint32_t test_page = NAND_TEST_BLOCK * STORAGE_PAGES_PER_BLOCK;
    ret = storage_hal_write_page(test_page, pattern);
    if (ret != ESP_OK) { ESP_LOGE(TAG, "Write falhou"); goto cleanup; }
    ESP_LOGI(TAG, "  Write %u bytes OK", STORAGE_PAGE_SIZE);

    ret = storage_hal_read_page(test_page, readback);
    if (ret != ESP_OK) { ESP_LOGE(TAG, "Read falhou"); goto cleanup; }
    ESP_LOGI(TAG, "  Read %u bytes OK", STORAGE_PAGE_SIZE);

    if (memcmp(pattern, readback, STORAGE_PAGE_SIZE) == 0) {
        ESP_LOGI(TAG, "POST PASS — todos os %u bytes do padrao bateram", STORAGE_PAGE_SIZE);
        ret = ESP_OK;
    } else {
        for (size_t i = 0; i < STORAGE_PAGE_SIZE; ++i) {
            if (pattern[i] != readback[i]) {
                ESP_LOGE(TAG, "POST FAIL — primeiro mismatch no offset %u (esperado 0x%02X, lido 0x%02X)",
                         (unsigned)i, pattern[i], readback[i]);
                break;
            }
        }
        ret = ESP_FAIL;
    }

cleanup:
    free(pattern);
    free(readback);
    return ret;
}

/* --- Suite de validacao fisica --- */

static void storage_dump_registers(void)
{
    uint8_t prot = 0xFF, conf = 0xFF, stat = 0xFF;
    nand_get_feature(NAND_REG_PROTECTION, &prot);
    nand_get_feature(NAND_REG_CONFIG,     &conf);
    nand_get_feature(NAND_REG_STATUS,     &stat);

    ESP_LOGI(TAG, "  Protection (0xA0) = 0x%02X — SRP0=%u BP=%u%u%u TB=%u WP-E=%u",
             prot,
             (prot >> 7) & 1,
             (prot >> 6) & 1, (prot >> 5) & 1, (prot >> 4) & 1,
             (prot >> 3) & 1,
             (prot >> 2) & 1);
    ESP_LOGI(TAG, "  Config     (0xB0) = 0x%02X — OTP-L=%u OTP-E=%u SR1-L=%u ECC-E=%u BUF=%u",
             conf,
             (conf >> 7) & 1, (conf >> 6) & 1, (conf >> 5) & 1,
             (conf >> 4) & 1, (conf >> 3) & 1);
    ESP_LOGI(TAG, "  Status     (0xC0) = 0x%02X — OIP=%u WEL=%u E_FAIL=%u P_FAIL=%u ECCS=%u%u",
             stat,
             stat & 1, (stat >> 1) & 1, (stat >> 2) & 1, (stat >> 3) & 1,
             (stat >> 5) & 1, (stat >> 4) & 1);
}

static esp_err_t storage_test_multi_region(void)
{
    static const uint32_t test_blocks[] = { 0, 256, 512, 768, 1023 };
    const size_t n = sizeof(test_blocks) / sizeof(test_blocks[0]);

    uint8_t *pattern  = heap_caps_malloc(STORAGE_PAGE_SIZE, MALLOC_CAP_DMA);
    uint8_t *readback = heap_caps_malloc(STORAGE_PAGE_SIZE, MALLOC_CAP_DMA);
    if (pattern == NULL || readback == NULL) {
        free(pattern); free(readback);
        ESP_LOGE(TAG, "  Falha ao alocar buffers de teste");
        return ESP_ERR_NO_MEM;
    }

    uint32_t pass = 0;
    char fail_list[96] = { 0 };
    size_t fpos = 0;

    for (size_t i = 0; i < n; ++i) {
        const uint32_t b = test_blocks[i];

        if (storage_hal_is_block_bad(b)) {
            ESP_LOGW(TAG, "  Bloco %u marcado bad — pulando", (unsigned)b);
            continue;
        }

        /* Padrao unico por bloco (id no XOR) — pega cross-block contamination. */
        for (size_t j = 0; j < STORAGE_PAGE_SIZE; ++j) {
            pattern[j] = (uint8_t)(b ^ j);
        }
        const uint32_t test_page = b * STORAGE_PAGES_PER_BLOCK;

        bool ok = (storage_hal_erase_block(b) == ESP_OK)
               && (storage_hal_write_page(test_page, pattern) == ESP_OK)
               && (storage_hal_read_page(test_page, readback) == ESP_OK)
               && (memcmp(pattern, readback, STORAGE_PAGE_SIZE) == 0);

        if (ok) {
            pass++;
        } else if (fpos < sizeof(fail_list) - 8) {
            fpos += snprintf(fail_list + fpos, sizeof(fail_list) - fpos,
                             "%s%u", fpos == 0 ? "" : ", ", (unsigned)b);
        }
    }

    free(pattern);
    free(readback);

    if (pass == n) {
        ESP_LOGI(TAG, "  Multi-region: %u/%u blocos OK (0, 256, 512, 768, 1023)",
                 (unsigned)pass, (unsigned)n);
        return ESP_OK;
    }
    ESP_LOGE(TAG, "  Multi-region: %u/%u blocos OK — falharam: %s",
             (unsigned)pass, (unsigned)n, fail_list);
    return ESP_FAIL;
}

static esp_err_t storage_scan_bad_blocks(void)
{
    uint32_t bad_count = 0;
    char bad_list[256] = { 0 };
    size_t pos = 0;
    bool truncated = false;

    for (uint32_t b = 0; b < STORAGE_BLOCK_COUNT; ++b) {
        if (!storage_hal_is_block_bad(b)) continue;

        bad_count++;
        if (truncated) continue;

        const int written = snprintf(bad_list + pos, sizeof(bad_list) - pos,
                                     "%s%u", pos == 0 ? "" : ", ", (unsigned)b);
        if (written < 0 || (size_t)written >= sizeof(bad_list) - pos) {
            truncated = true;
        } else {
            pos += (size_t)written;
        }
    }

    if (bad_count == 0) {
        ESP_LOGI(TAG, "  Bad block scan: 0/%u ruim (chip integro)", STORAGE_BLOCK_COUNT);
    } else {
        ESP_LOGI(TAG, "  Bad block scan: %u/%u ruim — blocos: %s%s",
                 (unsigned)bad_count, STORAGE_BLOCK_COUNT,
                 bad_list, truncated ? " ..." : "");
    }
    return ESP_OK;
}

esp_err_t storage_hal_run_full_validation(void)
{
    if (s_spi == NULL) return ESP_ERR_INVALID_STATE;

    ESP_LOGW(TAG, "Suite de validacao DESTRUTIVA nos blocos 0, 256, 512, 768, 1023.");

    ESP_LOGI(TAG, "[1/3] Feature registers:");
    storage_dump_registers();

    ESP_LOGI(TAG, "[2/3] Multi-region write/read test:");
    const esp_err_t r2 = storage_test_multi_region();

    ESP_LOGI(TAG, "[3/3] Full bad block scan (pode levar ~5s):");
    const esp_err_t r3 = storage_scan_bad_blocks();

    if (r2 == ESP_OK && r3 == ESP_OK) {
        ESP_LOGI(TAG, "Validacao completa: tudo OK");
        return ESP_OK;
    }
    ESP_LOGE(TAG, "Validacao completa: alguma etapa falhou");
    return ESP_FAIL;
}
