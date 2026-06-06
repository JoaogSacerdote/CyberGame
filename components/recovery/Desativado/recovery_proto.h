#pragma once
/* Contrato do protocolo de transferencia de assets via USB CDC (modo recovery).
 *
 * Este header e a FONTE DA VERDADE do protocolo. O uploader Python da Fase 4
 * (tools/asset_uploader.py) deve espelhar exatamente estas constantes e layouts.
 *
 * ---- Formato do frame (PC <-> ESP, ambas as direcoes) ----
 *
 *   off   size  campo
 *   0     1     SOF         = 0xA5
 *   1     1     type        codigo de comando (PC->ESP) ou resposta (ESP->PC)
 *   2     2     payload_len uint16 little-endian
 *   4     N     payload     N = payload_len bytes
 *   4+N   4     crc32       uint32 LE = esp_rom_crc32_le(0, &type, 3 + N)
 *                            (cobre type + payload_len + payload; NAO cobre o SOF)
 *
 * ---- Regras ----
 *  - Toda transferencia e iniciada pelo PC; o ESP so emite frames de resposta.
 *  - Todo comando termina com um frame RESP_ACK (sucesso) ou RESP_NACK (falha).
 *  - CMD_LIST: ESP responde 0..N frames RESP_INFO, depois RESP_ACK.
 *  - CMD_GET:  ESP responde 0..N frames RESP_DATA, depois RESP_ACK
 *              (ou direto RESP_NACK se o asset nao existe).
 *  - CMD_PUT:  PUT_BEGIN -> (PUT_DATA)* -> PUT_END. Cada frame recebe ACK/NACK.
 *              Qualquer NACK durante a sessao a aborta no lado do ESP.
 *  - CRC de frame invalido -> RESP_NACK(ESP_ERR_INVALID_CRC), frame descartado.
 *
 * ---- Integridade do asset ----
 *  - O campo `crc` do PUT_BEGIN e o esp_rom_crc32_le(0, payload, size) de TODO
 *    o asset. No PUT_END o ESP compara com o CRC que o asset_store acumulou e
 *    devolve RESP_NACK(ESP_ERR_INVALID_CRC) se divergir (checagem fim-a-fim).
 */

#include <stdint.h>

#define RECOVERY_PROTO_VERSION   1u

#define RECOVERY_SOF             0xA5u

/* Maior payload de um frame. Casado com o tamanho de pagina da W25N01GV
 * (2048 B) — chunk natural de CMD_PUT_DATA / RESP_DATA. */
#define RECOVERY_MAX_PAYLOAD     2048u

/* SOF + type + payload_len(2) + crc(4). */
#define RECOVERY_FRAME_OVERHEAD  8u
#define RECOVERY_FRAME_MAX       (RECOVERY_FRAME_OVERHEAD + RECOVERY_MAX_PAYLOAD)

/* Nome maximo de asset — DEVE casar com ASSET_NAME_MAX do asset_store.h. */
#define RECOVERY_NAME_MAX        40

/* ---- Comandos: PC -> ESP (faixa 0x01..0x7F) ---- */
#define CMD_PING            0x01u  /* payload vazio                           */
#define CMD_LIST            0x02u  /* payload vazio                           */
#define CMD_PUT_BEGIN       0x03u  /* payload: recovery_put_begin_t           */
#define CMD_PUT_DATA        0x04u  /* payload: bytes crus do asset (1..2048)  */
#define CMD_PUT_END         0x05u  /* payload vazio -> commit + verifica CRC  */
#define CMD_PUT_ABORT       0x06u  /* payload vazio -> aborta a sessao        */
#define CMD_GET             0x07u  /* payload: recovery_get_req_t             */
#define CMD_ERASE_CAT       0x08u  /* payload: 1 byte = asset_type_t          */
#define CMD_FACTORY_RESET   0x09u  /* payload vazio                           */
#define CMD_SELFTEST        0x0Au  /* payload vazio -> validacao fisica do NAND.
                                    * DESTRUTIVA nos blocos de diagnostico
                                    * 0/256/512/768/1023; leva ~5-10 s.        */

/* ---- Respostas: ESP -> PC (faixa 0x80..0xFF) ---- */
#define RESP_ACK            0x80u  /* sucesso; payload vazio                  */
#define RESP_NACK           0x81u  /* falha; payload: int32 LE = esp_err_t    */
#define RESP_PONG           0x82u  /* resposta de PING; payload: 1 byte versao*/
#define RESP_INFO           0x83u  /* 1 entrada de LIST; recovery_asset_info_t*/
#define RESP_DATA           0x84u  /* 1 chunk de GET; bytes crus do asset     */

/* ---- Layouts de payload ----
 * Empacotados (sem padding) e little-endian. ESP32-S3 e x86 sao ambos LE,
 * entao o uploader Python pode usar struct.pack/unpack com '<' diretamente. */

typedef struct __attribute__((packed)) {
    uint8_t  type;                       /* asset_type_t                      */
    uint16_t id;
    uint32_t size;                       /* total de bytes do asset           */
    uint32_t crc;                        /* esp_rom_crc32_le(0, payload, size) */
    char     name[RECOVERY_NAME_MAX];    /* nao precisa terminar em '\0'      */
} recovery_put_begin_t;                  /* 51 bytes */

typedef struct __attribute__((packed)) {
    uint8_t  type;                       /* asset_type_t                      */
    uint16_t id;
} recovery_get_req_t;                    /* 3 bytes */

typedef struct __attribute__((packed)) {
    uint8_t  type;                       /* asset_type_t                      */
    uint16_t id;
    uint32_t size;
    uint32_t crc;
    char     name[RECOVERY_NAME_MAX];
} recovery_asset_info_t;                 /* 51 bytes — payload de RESP_INFO */
