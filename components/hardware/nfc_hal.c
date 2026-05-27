#include "nfc_hal.h"
#include "board_pins.h"
#include "hal_common.h"

#include <assert.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "driver/gpio.h"
#include "driver/i2c_master.h"
#include "esp_check.h"
#include "esp_log.h"

static const char *TAG = "NFC_HAL";

#define NFC_I2C_PORT            I2C_NUM_0
#define NFC_I2C_FREQ_HZ         100000
#define NFC_I2C_ADDR            0x24        /* PN532 endereco 7-bit */
#define NFC_I2C_TIMEOUT_MS      100

#define PN532_TFI_H2D           0xD4
#define PN532_TFI_D2H           0xD5
#define PN532_CMD_GETFW         0x02
#define PN532_CMD_SAMCONFIG     0x14
#define PN532_CMD_RFCONFIG      0x32
#define PN532_CMD_INLISTPASSIVE 0x4A

#define NFC_FRAME_BUF_LEN       64          /* maior que qualquer resposta esperada */
#define NFC_QUEUE_DEPTH         4

#define NFC_POLL_PERIOD_MS      200
#define NFC_RESP_TIMEOUT_MS     200

#define NFC_TASK_STACK          4096
#define NFC_TASK_PRIO           4

static const uint8_t PN532_ACK[] = { 0x00, 0x00, 0xFF, 0x00, 0xFF, 0x00 };

static i2c_master_bus_handle_t s_bus      = NULL;
static i2c_master_dev_handle_t s_dev      = NULL;
static SemaphoreHandle_t       s_irq      = NULL;
static QueueHandle_t           s_queue    = NULL;
static SemaphoreHandle_t       s_wakeup   = NULL;
static volatile bool           s_scanning = false;

static void IRAM_ATTR nfc_irq_isr(void *arg)
{
    (void)arg;
    BaseType_t hp_woken = pdFALSE;
    xSemaphoreGiveFromISR(s_irq, &hp_woken);
    if (hp_woken == pdTRUE) {
        portYIELD_FROM_ISR();
    }
}

/* Monta uma frame normal de comando do PN532:
 * 00 00 FF [LEN] [LCS] [TFI=D4] [CMD] [PD0..PDn] [DCS] 00
 * Onde LCS = ~LEN+1 e DCS = ~(TFI+CMD+PDi)+1, ambos modulo 256. */
static int pn532_build_frame(uint8_t cmd,
                             const uint8_t *params, size_t params_len,
                             uint8_t *out, size_t out_size)
{
    if (params_len > NFC_FRAME_BUF_LEN - 9) return -1;
    if (out_size < params_len + 9) return -1;

    const uint8_t len = (uint8_t)(2 + params_len);
    out[0] = 0x00;
    out[1] = 0x00;
    out[2] = 0xFF;
    out[3] = len;
    out[4] = (uint8_t)(~len + 1);
    out[5] = PN532_TFI_H2D;
    out[6] = cmd;

    uint8_t sum = PN532_TFI_H2D + cmd;
    for (size_t i = 0; i < params_len; ++i) {
        out[7 + i] = params[i];
        sum += params[i];
    }
    out[7 + params_len] = (uint8_t)(~sum + 1);
    out[8 + params_len] = 0x00;

    return (int)(params_len + 9);
}

/* I2C reads do PN532 sao prefixados por byte de status: 0x01=pronto, 0x00=ainda nao. */
static esp_err_t pn532_read_after_irq(uint8_t *out, size_t out_len, uint32_t timeout_ms)
{
    if (xSemaphoreTake(s_irq, pdMS_TO_TICKS(timeout_ms)) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }

    uint8_t buf[NFC_FRAME_BUF_LEN + 1];
    if (out_len + 1 > sizeof(buf)) return ESP_ERR_INVALID_SIZE;

    esp_err_t err = i2c_master_receive(s_dev, buf, out_len + 1, NFC_I2C_TIMEOUT_MS);
    if (err != ESP_OK) return err;
    if (buf[0] != 0x01) return ESP_ERR_NOT_FOUND;

    memcpy(out, &buf[1], out_len);
    return ESP_OK;
}

static esp_err_t pn532_wait_ack(uint32_t timeout_ms)
{
    uint8_t buf[sizeof(PN532_ACK)];
    esp_err_t err = pn532_read_after_irq(buf, sizeof(buf), timeout_ms);
    if (err != ESP_OK) return err;
    return (memcmp(buf, PN532_ACK, sizeof(PN532_ACK)) == 0) ? ESP_OK : ESP_ERR_INVALID_RESPONSE;
}

/* Envia comando e devolve payload da resposta (bytes apos 0xD5+cmd_resp).
 * Retorna numero de bytes do payload, ou negativo em erro. */
static int pn532_command(uint8_t cmd,
                         const uint8_t *params, size_t params_len,
                         uint8_t *resp_payload, size_t resp_payload_size)
{
    /* Drena qualquer sinal de IRQ remanescente de uma transacao anterior
     * abortada — senao o proximo Take retorna imediatamente sem dados reais. */
    while (xSemaphoreTake(s_irq, 0) == pdTRUE) { /* drain */ }

    uint8_t frame[NFC_FRAME_BUF_LEN];
    const int frame_len = pn532_build_frame(cmd, params, params_len, frame, sizeof(frame));
    if (frame_len < 0) return -1;

    if (i2c_master_transmit(s_dev, frame, (size_t)frame_len, NFC_I2C_TIMEOUT_MS) != ESP_OK) {
        return -2;
    }
    if (pn532_wait_ack(NFC_RESP_TIMEOUT_MS) != ESP_OK) {
        return -3;
    }

    uint8_t raw[NFC_FRAME_BUF_LEN];
    if (pn532_read_after_irq(raw, sizeof(raw), NFC_RESP_TIMEOUT_MS) != ESP_OK) {
        return -4;
    }

    if (raw[0] != 0x00 || raw[1] != 0x00 || raw[2] != 0xFF) return -5;
    const uint8_t len = raw[3];
    if (((uint8_t)(raw[3] + raw[4])) != 0)                  return -6;
    if (raw[5] != PN532_TFI_D2H)                            return -7;
    if (raw[6] != (uint8_t)(cmd + 1))                       return -8;

    const int payload = (int)len - 2;
    if (payload < 0 || payload > (int)resp_payload_size)    return -9;

    memcpy(resp_payload, &raw[7], (size_t)payload);
    return payload;
}

static esp_err_t pn532_get_firmware(uint8_t *ic, uint8_t *ver, uint8_t *rev, uint8_t *sup)
{
    uint8_t resp[4];
    if (pn532_command(PN532_CMD_GETFW, NULL, 0, resp, sizeof(resp)) != 4) return ESP_FAIL;
    *ic = resp[0]; *ver = resp[1]; *rev = resp[2]; *sup = resp[3];
    return ESP_OK;
}

static esp_err_t pn532_sam_configure(void)
{
    /* Modo 0x01 = normal, timeout 0x14 = 1s, IRQ habilitado (0x01). */
    const uint8_t params[] = { 0x01, 0x14, 0x01 };
    uint8_t resp[1];
    return (pn532_command(PN532_CMD_SAMCONFIG, params, sizeof(params), resp, sizeof(resp)) >= 0)
               ? ESP_OK : ESP_FAIL;
}

/* RFConfiguration item 0x05 (MaxRetries):
 *   [0x05] [MxRtyATR=0xFF] [MxRtyPSL=0x01] [MxRtyPassiveActivation]
 * Sem isso, MxRtyPassiveActivation default eh 0xFF (infinito) — InListPassiveTarget
 * fica buscando para sempre e a primeira leitura demora ate o PN532 desbloquear
 * sozinho. Setando 0x01: tenta uma vez (~16ms) e desiste por ciclo de poll. */
static esp_err_t pn532_set_max_retries(uint8_t mx_passive_activation)
{
    const uint8_t params[] = { 0x05, 0xFF, 0x01, mx_passive_activation };
    uint8_t resp[1];
    return (pn532_command(PN532_CMD_RFCONFIG, params, sizeof(params), resp, sizeof(resp)) >= 0)
               ? ESP_OK : ESP_FAIL;
}

/* Retorna 1 se cartao detectado, 0 se nao, <0 em erro. */
static int pn532_poll_iso14443a(nfc_card_t *card)
{
    const uint8_t params[] = { 0x01, 0x00 };    /* MaxTg=1, BrTy=ISO14443A */
    uint8_t resp[32];
    const int len = pn532_command(PN532_CMD_INLISTPASSIVE, params, sizeof(params),
                                  resp, sizeof(resp));
    if (len < 1)         return -1;
    if (resp[0] == 0)    return 0;
    if (resp[0] != 1)    return -2;
    if (len < 6)         return -3;

    card->atqa    = ((uint16_t)resp[2] << 8) | resp[3];
    card->sak     = resp[4];
    card->uid_len = resp[5];
    if (card->uid_len > NFC_UID_MAX_LEN || len < 6 + card->uid_len) return -4;

    memcpy(card->uid, &resp[6], card->uid_len);
    return 1;
}

static bool same_uid(const nfc_card_t *a, const nfc_card_t *b)
{
    return a->uid_len == b->uid_len && memcmp(a->uid, b->uid, a->uid_len) == 0;
}

static void nfc_task(void *pv)
{
    (void)pv;
    nfc_card_t last      = { 0 };
    bool       last_held = false;

    while (1) {
        if (!s_scanning) {
            /* Bloqueia eficientemente ate start_scanning liberar o semaforo. */
            xSemaphoreTake(s_wakeup, portMAX_DELAY);
            last_held = false;  /* reinicio fresco — proximo cartao eh evento novo */
            continue;
        }

        nfc_card_t card = { 0 };
        const int r = pn532_poll_iso14443a(&card);
        if (r == 1) {
            if (!last_held || !same_uid(&last, &card)) {
                xQueueSend(s_queue, &card, 0);
                last      = card;
                last_held = true;
            }
        } else if (r == 0) {
            last_held = false;
        } else {
            ESP_LOGW(TAG, "poll falhou (r=%d) — retentando no proximo ciclo", r);
        }
        vTaskDelay(pdMS_TO_TICKS(NFC_POLL_PERIOD_MS));
    }
}

esp_err_t nfc_hal_init(void)
{
    if (s_queue != NULL) {
        return ESP_OK;
    }

    s_irq    = xSemaphoreCreateBinary();
    ESP_RETURN_ON_FALSE(s_irq    != NULL, ESP_ERR_NO_MEM, TAG, "irq sem alloc failed");

    s_wakeup = xSemaphoreCreateBinary();
    ESP_RETURN_ON_FALSE(s_wakeup != NULL, ESP_ERR_NO_MEM, TAG, "wakeup sem alloc failed");

    s_queue  = xQueueCreate(NFC_QUEUE_DEPTH, sizeof(nfc_card_t));
    ESP_RETURN_ON_FALSE(s_queue  != NULL, ESP_ERR_NO_MEM, TAG, "queue alloc failed");

    const i2c_master_bus_config_t bus_cfg = {
        .i2c_port                     = NFC_I2C_PORT,
        .sda_io_num                   = BOARD_PIN_NFC_SDA,
        .scl_io_num                   = BOARD_PIN_NFC_SCL,
        .clk_source                   = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt            = 7,
        .flags.enable_internal_pullup = true,
    };
    ESP_RETURN_ON_ERROR(i2c_new_master_bus(&bus_cfg, &s_bus), TAG, "i2c bus init failed");

    const i2c_device_config_t dev_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address  = NFC_I2C_ADDR,
        .scl_speed_hz    = NFC_I2C_FREQ_HZ,
    };
    ESP_RETURN_ON_ERROR(i2c_master_bus_add_device(s_bus, &dev_cfg, &s_dev),
                        TAG, "i2c device add failed");

    const gpio_config_t irq_cfg = {
        .pin_bit_mask = 1ULL << BOARD_PIN_NFC_IRQ,
        .mode         = GPIO_MODE_INPUT,
        .pull_up_en   = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type    = GPIO_INTR_NEGEDGE,
    };
    ESP_RETURN_ON_ERROR(gpio_config(&irq_cfg), TAG, "irq pin config failed");

    ESP_RETURN_ON_ERROR(hal_isr_service_install_once(), TAG, "isr service install failed");
    ESP_RETURN_ON_ERROR(gpio_isr_handler_add(BOARD_PIN_NFC_IRQ, nfc_irq_isr, NULL),
                        TAG, "isr handler add failed");

    /* PN532 acorda na primeira atividade I2C — espera estabilizar antes do handshake. */
    vTaskDelay(pdMS_TO_TICKS(50));

    uint8_t ic, ver, rev, sup;
    ESP_RETURN_ON_ERROR(pn532_get_firmware(&ic, &ver, &rev, &sup),
                        TAG, "pn532 firmware version failed (PN532 nao respondeu)");
    ESP_LOGI(TAG, "PN532 detectado: IC=0x%02X firmware v%d.%d (suporte=0x%02X)",
             ic, ver, rev, sup);

    ESP_RETURN_ON_ERROR(pn532_sam_configure(), TAG, "pn532 SAM configure failed");

    /* Best-effort: se o PN532 nao aceitar a configuracao, fica com retries
     * default (lento na primeira leitura) — mas o boot nao pode quebrar por isso. */
    if (pn532_set_max_retries(0x01) != ESP_OK) {
        ESP_LOGW(TAG, "Falha ao reduzir MxRtyPassiveActivation — primeira leitura sera lenta. Continuando.");
    }

    const BaseType_t ok = xTaskCreate(nfc_task, "nfc_hal",
                                      NFC_TASK_STACK, NULL, NFC_TASK_PRIO, NULL);
    ESP_RETURN_ON_FALSE(ok == pdPASS, ESP_ERR_NO_MEM, TAG, "task create failed");

    ESP_LOGI(TAG, "nfc_hal initialized (SDA=GPIO%d SCL=GPIO%d IRQ=GPIO%d, %d kHz, poll %d ms, scan OFF)",
             BOARD_PIN_NFC_SDA, BOARD_PIN_NFC_SCL, BOARD_PIN_NFC_IRQ, NFC_I2C_FREQ_HZ / 1000, NFC_POLL_PERIOD_MS);
    return ESP_OK;
}

esp_err_t nfc_hal_start_scanning(void)
{
    if (s_wakeup == NULL) return ESP_ERR_INVALID_STATE;
    s_scanning = true;
    xSemaphoreGive(s_wakeup);
    return ESP_OK;
}

esp_err_t nfc_hal_stop_scanning(void)
{
    if (s_wakeup == NULL) return ESP_ERR_INVALID_STATE;
    s_scanning = false;
    return ESP_OK;
}

bool nfc_hal_wait_card(nfc_card_t *card, uint32_t timeout_ms)
{
    assert(card != NULL);
    if (s_queue == NULL) return false;
    const TickType_t ticks = (timeout_ms == UINT32_MAX) ? portMAX_DELAY
                                                        : pdMS_TO_TICKS(timeout_ms);
    return xQueueReceive(s_queue, card, ticks) == pdTRUE;
}
