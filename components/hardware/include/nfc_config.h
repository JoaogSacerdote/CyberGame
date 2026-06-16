#pragma once

#include <stdint.h>

/* === Mapeamento UID -> Carta de Defesa NFC ===
 *
 * Cada carta fisica pode ter varios UIDs registrados (duplicatas para reposicao).
 * O campo uid_len discrimina tags de 4 e 7 bytes — ambos suportados.
 * Para adicionar carta nova: copiar o UID do log "NFC UID desconhecido" e
 * acrescentar uma entrada abaixo incrementando NFC_CARTAS_COUNT.
 */

typedef enum {
    CARTA_ISOLAMENTO = 0,        /* contra Propagacao Lateral */
    CARTA_BACKUP,                /* contra Ransomware */
    CARTA_BALANCEAMENTO,         /* contra DDoS */
    CARTA_MAX_COUNT,
} carta_id_t;

typedef struct {
    uint8_t  uid[10];   /* espelha NFC_UID_MAX_LEN de nfc_hal.h */
    uint8_t  uid_len;
    carta_id_t carta;
    const char *nome;
} nfc_card_entry_t;

/* Numero total de tags cadastradas (>= CARTA_MAX_COUNT pois ha duplicatas). */
#define NFC_CARTAS_COUNT  12

static const nfc_card_entry_t NFC_CARTAS[NFC_CARTAS_COUNT] = {
    /* --- Isolamento de Rede (contra Propagacao Lateral) --- */
    { .uid = { 0x04, 0x43, 0x2D, 0x46 },                   .uid_len = 4,
      .carta = CARTA_ISOLAMENTO,    .nome = "Isolamento de Rede" },
    { .uid = { 0x04, 0x38, 0x2D, 0x46, 0xBC, 0x2A, 0x81 }, .uid_len = 7,
      .carta = CARTA_ISOLAMENTO,    .nome = "Isolamento de Rede" },
    { .uid = { 0x04, 0x31, 0x2D, 0x46, 0xBC, 0x2A, 0x81 }, .uid_len = 7,
      .carta = CARTA_ISOLAMENTO,    .nome = "Isolamento de Rede" },
    { .uid = { 0x81, 0x78, 0x58, 0x3E },                   .uid_len = 4,
      .carta = CARTA_ISOLAMENTO,    .nome = "Isolamento de Rede" },

    /* --- Backup de Emergencia (contra Ransomware) --- */
    { .uid = { 0x04, 0x39, 0x2D, 0x46 },                   .uid_len = 4,
      .carta = CARTA_BACKUP,        .nome = "Backup de Emergencia" },
    { .uid = { 0x04, 0x2E, 0x2D, 0x46, 0xBC, 0x2A, 0x81 }, .uid_len = 7,
      .carta = CARTA_BACKUP,        .nome = "Backup de Emergencia" },
    { .uid = { 0x04, 0x37, 0x2D, 0x46, 0xBC, 0x2A, 0x81 }, .uid_len = 7,
      .carta = CARTA_BACKUP,        .nome = "Backup de Emergencia" },
    { .uid = { 0x4E, 0x60, 0x6F, 0x36 },                   .uid_len = 4,
      .carta = CARTA_BACKUP,        .nome = "Backup de Emergencia" },

    /* --- Balanceamento de Rede (contra DDoS) --- */
    { .uid = { 0x04, 0x42, 0x2D, 0x46 },                   .uid_len = 4,
      .carta = CARTA_BALANCEAMENTO, .nome = "Balanceamento de Rede" },
    { .uid = { 0x04, 0x3A, 0x2D, 0x46, 0xBC, 0x2A, 0x81 }, .uid_len = 7,
      .carta = CARTA_BALANCEAMENTO, .nome = "Balanceamento de Rede" },
    { .uid = { 0x04, 0x2F, 0x2D, 0x46, 0xBC, 0x2A, 0x81 }, .uid_len = 7,
      .carta = CARTA_BALANCEAMENTO, .nome = "Balanceamento de Rede" },
    { .uid = { 0x26, 0x4E, 0x59, 0x3E },                   .uid_len = 4,
      .carta = CARTA_BALANCEAMENTO, .nome = "Balanceamento de Rede" },
};
