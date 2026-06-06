#include "recovery.h"
#include "recovery_proto.h"

#include <stdlib.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_check.h"
#include "esp_log.h"
#include "esp_rom_crc.h"
#include "tinyusb.h"
#include "tusb_cdc_acm.h"
#include "asset_store.h"
#include "storage_hal.h"

static const char *TAG = "RECOVERY";

/* ---- Buffers estaticos (sessao unica, loop unico — sem reentrancia) ---- */
static uint8_t s_rx[RECOVERY_FRAME_MAX];      /* frame em montagem: type..crc  */
static uint8_t s_tx[RECOVERY_FRAME_MAX];      /* frame de saida completo       */
static uint8_t s_chunk[RECOVERY_MAX_PAYLOAD]; /* chunk de leitura do GET       */

/* ---- Estado do parser de frame ---- */
static bool   s_in_frame;   /* SOF visto, montando frame                       */
static size_t s_got;        /* bytes ja recebidos em s_rx                      */
static size_t s_want;       /* total esperado: 3 + payload_len + 4             */

/* ---- Estado da sessao de escrita (CMD_PUT_*) ---- */
static bool                 s_put_active;
static asset_write_handle_t s_put_handle;
static uint32_t             s_put_crc;    /* CRC anunciado no PUT_BEGIN         */
static uint8_t              s_put_type;
static uint16_t             s_put_id;

/* ============================ I/O bruto CDC ============================ */

/* Escreve 'len' bytes no CDC, drenando a fila quantas vezes for preciso. */
static void cdc_write_all(const uint8_t *buf, size_t len)
{
    size_t sent = 0;
    while (sent < len) {
        size_t n = tinyusb_cdcacm_write_queue(TINYUSB_CDC_ACM_0,
                                              buf + sent, len - sent);
        if (n == 0) {
            tinyusb_cdcacm_write_flush(TINYUSB_CDC_ACM_0, pdMS_TO_TICKS(100));
            continue;
        }
        sent += n;
    }
    tinyusb_cdcacm_write_flush(TINYUSB_CDC_ACM_0, pdMS_TO_TICKS(100));
}

/* ============================ Envio de frames ========================== */

static void send_frame(uint8_t type, const void *payload, uint16_t plen)
{
    s_tx[0] = RECOVERY_SOF;
    s_tx[1] = type;
    s_tx[2] = (uint8_t)(plen & 0xFFu);
    s_tx[3] = (uint8_t)((plen >> 8) & 0xFFu);
    if (plen && payload) {
        memcpy(&s_tx[4], payload, plen);
    }
    const uint32_t crc = esp_rom_crc32_le(0, &s_tx[1], 3u + plen);
    const size_t   off = 4u + plen;
    s_tx[off + 0] = (uint8_t)(crc & 0xFFu);
    s_tx[off + 1] = (uint8_t)((crc >> 8) & 0xFFu);
    s_tx[off + 2] = (uint8_t)((crc >> 16) & 0xFFu);
    s_tx[off + 3] = (uint8_t)((crc >> 24) & 0xFFu);
    cdc_write_all(s_tx, off + 4u);
}

static void send_ack(void)
{
    send_frame(RESP_ACK, NULL, 0);
}

static void send_nack(esp_err_t err)
{
    const int32_t code = (int32_t)err;
    send_frame(RESP_NACK, &code, sizeof(code));
}

/* ============================ Handlers ================================= */

static void do_ping(void)
{
    const uint8_t version = RECOVERY_PROTO_VERSION;
    send_frame(RESP_PONG, &version, sizeof(version));
}

static void do_list(void)
{
    size_t count = 0;
    esp_err_t err = asset_store_count(&count);
    if (err != ESP_OK) {
        send_nack(err);
        return;
    }
    if (count > 0) {
        asset_info_t *arr = malloc(count * sizeof(asset_info_t));
        if (arr == NULL) {
            send_nack(ESP_ERR_NO_MEM);
            return;
        }
        size_t actual = 0;
        err = asset_store_list(arr, count, &actual);
        if (err != ESP_OK) {
            free(arr);
            send_nack(err);
            return;
        }
        for (size_t i = 0; i < actual; ++i) {
            recovery_asset_info_t ri;
            ri.type = (uint8_t)arr[i].type;
            ri.id   = arr[i].id;
            ri.size = arr[i].size;
            ri.crc  = arr[i].crc;
            memset(ri.name, 0, sizeof(ri.name));
            memcpy(ri.name, arr[i].name, RECOVERY_NAME_MAX);
            send_frame(RESP_INFO, &ri, sizeof(ri));
        }
        free(arr);
    }
    send_ack();
}

static void do_put_begin(const uint8_t *payload, uint16_t plen)
{
    /* Sessao anterior pendente? Aborta antes de iniciar a nova. */
    if (s_put_active) {
        asset_store_abort_write(s_put_handle);
        s_put_active = false;
    }
    if (plen < sizeof(recovery_put_begin_t)) {
        send_nack(ESP_ERR_INVALID_SIZE);
        return;
    }
    recovery_put_begin_t pb;
    memcpy(&pb, payload, sizeof(pb));
    if (pb.type >= ASSET_TYPE_MAX) {
        send_nack(ESP_ERR_INVALID_ARG);
        return;
    }
    char name[RECOVERY_NAME_MAX + 1];
    memcpy(name, pb.name, RECOVERY_NAME_MAX);
    name[RECOVERY_NAME_MAX] = '\0';

    esp_err_t err = asset_store_begin_write((asset_type_t)pb.type, pb.id,
                                            name, pb.size, &s_put_handle);
    if (err != ESP_OK) {
        send_nack(err);
        return;
    }
    s_put_active = true;
    s_put_crc    = pb.crc;
    s_put_type   = pb.type;
    s_put_id     = pb.id;
    ESP_LOGI(TAG, "PUT_BEGIN type=%u id=%u size=%u '%s'",
             pb.type, pb.id, (unsigned)pb.size, name);
    send_ack();
}

static void do_put_data(const uint8_t *payload, uint16_t plen)
{
    if (!s_put_active) {
        send_nack(ESP_ERR_INVALID_STATE);
        return;
    }
    esp_err_t err = asset_store_write_chunk(s_put_handle, payload, plen);
    if (err != ESP_OK) {
        asset_store_abort_write(s_put_handle);
        s_put_active = false;
        send_nack(err);
        return;
    }
    send_ack();
}

static void do_put_end(void)
{
    if (!s_put_active) {
        send_nack(ESP_ERR_INVALID_STATE);
        return;
    }
    esp_err_t err = asset_store_commit_write(s_put_handle);
    s_put_active = false;
    if (err != ESP_OK) {
        send_nack(err);
        return;
    }
    /* Verificacao fim-a-fim: o CRC gravado deve bater com o do PUT_BEGIN. */
    asset_info_t info;
    err = asset_store_get_info((asset_type_t)s_put_type, s_put_id, &info);
    if (err != ESP_OK) {
        send_nack(err);
        return;
    }
    if (info.crc != s_put_crc) {
        ESP_LOGE(TAG, "PUT_END CRC mismatch: gravado=0x%08X anunciado=0x%08X",
                 (unsigned)info.crc, (unsigned)s_put_crc);
        send_nack(ESP_ERR_INVALID_CRC);
        return;
    }
    ESP_LOGI(TAG, "PUT_END ok: type=%u id=%u size=%u crc=0x%08X",
             s_put_type, s_put_id, (unsigned)info.size, (unsigned)info.crc);
    send_ack();
}

static void do_put_abort(void)
{
    if (s_put_active) {
        asset_store_abort_write(s_put_handle);
        s_put_active = false;
    }
    send_ack();
}

static void do_get(const uint8_t *payload, uint16_t plen)
{
    if (plen < sizeof(recovery_get_req_t)) {
        send_nack(ESP_ERR_INVALID_SIZE);
        return;
    }
    recovery_get_req_t req;
    memcpy(&req, payload, sizeof(req));
    if (req.type >= ASSET_TYPE_MAX) {
        send_nack(ESP_ERR_INVALID_ARG);
        return;
    }
    asset_info_t info;
    esp_err_t err = asset_store_get_info((asset_type_t)req.type, req.id, &info);
    if (err != ESP_OK) {
        send_nack(err);
        return;
    }
    size_t off = 0;
    while (off < info.size) {
        size_t n = info.size - off;
        if (n > RECOVERY_MAX_PAYLOAD) {
            n = RECOVERY_MAX_PAYLOAD;
        }
        err = asset_store_read((asset_type_t)req.type, req.id, off, s_chunk, n);
        if (err != ESP_OK) {
            send_nack(err);
            return;
        }
        send_frame(RESP_DATA, s_chunk, (uint16_t)n);
        off += n;
    }
    send_ack();
}

static void do_erase_cat(const uint8_t *payload, uint16_t plen)
{
    if (plen < 1) {
        send_nack(ESP_ERR_INVALID_SIZE);
        return;
    }
    if (payload[0] >= ASSET_TYPE_MAX) {
        send_nack(ESP_ERR_INVALID_ARG);
        return;
    }
    esp_err_t err = asset_store_erase_category((asset_type_t)payload[0]);
    if (err != ESP_OK) {
        send_nack(err);
        return;
    }
    ESP_LOGI(TAG, "ERASE_CAT type=%u ok", payload[0]);
    send_ack();
}

static void do_factory_reset(void)
{
    esp_err_t err = asset_store_factory_reset();
    if (err != ESP_OK) {
        send_nack(err);
        return;
    }
    ESP_LOGW(TAG, "FACTORY_RESET concluido — NAND de assets zerada.");
    send_ack();
}

static void do_selftest(void)
{
    /* Validacao fisica do NAND — ferramenta de diagnostico, sob demanda.
     * Destrutiva nos blocos 0/256/512/768/1023 (diagnostico, fora da area
     * de assets do asset_store). Por isso NAO roda no boot de recovery. */
    ESP_LOGW(TAG, "SELFTEST: validacao fisica do NAND (~5-10 s, destrutiva nos "
                  "blocos de diagnostico)...");
    esp_err_t err = storage_hal_run_full_validation();
    if (err != ESP_OK) {
        send_nack(err);
        return;
    }
    ESP_LOGI(TAG, "SELFTEST concluido.");
    send_ack();
}

/* ============================ Dispatch ================================= */

static void dispatch(uint8_t type, const uint8_t *payload, uint16_t plen)
{
    switch (type) {
    case CMD_PING:          do_ping();                    break;
    case CMD_LIST:          do_list();                    break;
    case CMD_PUT_BEGIN:     do_put_begin(payload, plen);   break;
    case CMD_PUT_DATA:      do_put_data(payload, plen);    break;
    case CMD_PUT_END:       do_put_end();                  break;
    case CMD_PUT_ABORT:     do_put_abort();                break;
    case CMD_GET:           do_get(payload, plen);         break;
    case CMD_ERASE_CAT:     do_erase_cat(payload, plen);   break;
    case CMD_FACTORY_RESET: do_factory_reset();            break;
    case CMD_SELFTEST:      do_selftest();                 break;
    default:
        ESP_LOGW(TAG, "Comando desconhecido: 0x%02X", type);
        send_nack(ESP_ERR_NOT_SUPPORTED);
        break;
    }
}

/* Valida o CRC do frame montado em s_rx e despacha. */
static void handle_frame(void)
{
    const uint8_t  type = s_rx[0];
    const uint16_t plen = (uint16_t)s_rx[1] | ((uint16_t)s_rx[2] << 8);
    const uint8_t *payload = &s_rx[3];

    const uint32_t rx_crc = (uint32_t)s_rx[3 + plen]
                          | ((uint32_t)s_rx[3 + plen + 1] << 8)
                          | ((uint32_t)s_rx[3 + plen + 2] << 16)
                          | ((uint32_t)s_rx[3 + plen + 3] << 24);
    const uint32_t calc = esp_rom_crc32_le(0, &s_rx[0], 3u + plen);
    if (calc != rx_crc) {
        ESP_LOGW(TAG, "CRC de frame invalido (type=0x%02X): rx=0x%08X calc=0x%08X",
                 type, (unsigned)rx_crc, (unsigned)calc);
        send_nack(ESP_ERR_INVALID_CRC);
        return;
    }
    dispatch(type, payload, plen);
}

/* Alimenta o parser com 1 byte vindo do CDC. */
static void feed_byte(uint8_t c)
{
    if (!s_in_frame) {
        if (c == RECOVERY_SOF) {
            s_in_frame = true;
            s_got = 0;
            s_want = 3;   /* type + payload_len(2) — minimo pra saber o resto */
        }
        return;
    }

    s_rx[s_got++] = c;

    if (s_got == 3) {
        const uint16_t plen = (uint16_t)s_rx[1] | ((uint16_t)s_rx[2] << 8);
        if (plen > RECOVERY_MAX_PAYLOAD) {
            ESP_LOGW(TAG, "payload_len %u acima do maximo — frame descartado", plen);
            send_nack(ESP_ERR_INVALID_SIZE);
            s_in_frame = false;
            return;
        }
        s_want = 3u + plen + 4u;
    }

    if (s_got >= 3 && s_got == s_want) {
        s_in_frame = false;
        handle_frame();
        return;
    }

    if (s_got >= sizeof(s_rx)) {   /* salvaguarda contra estouro */
        ESP_LOGW(TAG, "frame estourou o buffer — ressincronizando");
        s_in_frame = false;
    }
}

/* ============================ API publica ============================== */

esp_err_t recovery_init(void)
{
    const tinyusb_config_t tusb_cfg = {
        .device_descriptor        = NULL,
        .string_descriptor        = NULL,
        .external_phy             = false,
        .configuration_descriptor = NULL,
    };
    ESP_RETURN_ON_ERROR(tinyusb_driver_install(&tusb_cfg), TAG, "tinyusb install failed");

    const tinyusb_config_cdcacm_t acm_cfg = {
        .usb_dev                       = TINYUSB_USBDEV_0,
        .cdc_port                      = TINYUSB_CDC_ACM_0,
        .rx_unread_buf_sz              = 256,
        .callback_rx                   = NULL,
        .callback_rx_wanted_char       = NULL,
        .callback_line_state_changed   = NULL,
        .callback_line_coding_changed  = NULL,
    };
    ESP_RETURN_ON_ERROR(tusb_cdc_acm_init(&acm_cfg), TAG, "cdc init failed");

    ESP_LOGI(TAG, "USB CDC pronto — protocolo de assets v%u, payload max %u B.",
             RECOVERY_PROTO_VERSION, RECOVERY_MAX_PAYLOAD);
    return ESP_OK;
}

void recovery_run(void)
{
    uint8_t rx_buf[64];

    s_in_frame   = false;
    s_put_active = false;

    ESP_LOGI(TAG, "Entrando em loop CDC. Aguardando frames do PC...");

    while (1) {
        size_t rx_len = 0;
        const esp_err_t err = tinyusb_cdcacm_read(TINYUSB_CDC_ACM_0,
                                                  rx_buf, sizeof(rx_buf), &rx_len);
        if (err == ESP_OK && rx_len > 0) {
            for (size_t i = 0; i < rx_len; ++i) {
                feed_byte(rx_buf[i]);
            }
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}
