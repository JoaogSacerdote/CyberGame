#include "storage_hal.h"

#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/spi_master.h"
#include "esp_check.h"
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
#define NAND_CMD_RESET         0xFF
#define NAND_CMD_JEDEC_ID      0x9F

/* JEDEC ID esperado: Winbond W25N01GV = EF AA 21 */
#define NAND_EXPECTED_MANUF    0xEF
#define NAND_EXPECTED_DEV_HI   0xAA
#define NAND_EXPECTED_DEV_LO   0x21

static spi_device_handle_t s_spi = NULL;

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

    return ESP_OK;
}
