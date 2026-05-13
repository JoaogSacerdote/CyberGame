#pragma once

#include <stdint.h>

/* === Mapeamento UID -> Carta de Defesa NFC ===
 *
 * Os UIDs sao HARDCODED conforme decisao em RESPOSTAS.txt (item 6 das
 * sub-duvidas). Para preencher os valores reais:
 *
 *   1. Boot em recovery (PWR+REC) e use o comando de leitura NFC bruta,
 *      OU use um app celular tipo "NFC TagWriter" para ler o UID de cada tag.
 *   2. Substitua os 0xDEADBEEFs abaixo pelos UIDs reais.
 *   3. Confirme o uid_len (MIFARE Classic costuma ser 4 ou 7 bytes).
 *
 * TODO(usuario): preencher os 3 UIDs reais antes da Etapa C entregar
 * gameplay funcional. Ate la, o sistema compila mas nenhuma carta sera
 * reconhecida como CORRETA.
 */

typedef enum {
    CARTA_ISOLAMENTO = 0,        /* contra Propagacao Lateral */
    CARTA_BACKUP,                /* contra Ransomware */
    CARTA_BALANCEAMENTO,         /* contra DDoS */
    CARTA_MAX_COUNT,
} carta_id_t;

typedef struct {
    uint8_t  uid[8];
    uint8_t  uid_len;
    carta_id_t carta;
    const char *nome;
} nfc_card_entry_t;

/* PLACEHOLDER — preencher com UIDs reais lidos das tags fisicas. */
static const nfc_card_entry_t NFC_CARTAS[CARTA_MAX_COUNT] = {
    { .uid = { 0xDE, 0xAD, 0xBE, 0xEF, 0x00, 0x00, 0x00, 0x00 }, .uid_len = 4,
      .carta = CARTA_ISOLAMENTO,    .nome = "Isolamento de Rede" },
    { .uid = { 0xDE, 0xAD, 0xBE, 0xEF, 0x01, 0x00, 0x00, 0x00 }, .uid_len = 4,
      .carta = CARTA_BACKUP,        .nome = "Backup de Emergencia" },
    { .uid = { 0xDE, 0xAD, 0xBE, 0xEF, 0x02, 0x00, 0x00, 0x00 }, .uid_len = 4,
      .carta = CARTA_BALANCEAMENTO, .nome = "Balanceamento de Rede" },
};
