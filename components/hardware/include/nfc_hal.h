#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

#define NFC_UID_MAX_LEN     10      /* PN532 suporta UIDs de 4, 7 ou 10 bytes (ISO14443A). */

typedef struct {
    uint8_t  uid[NFC_UID_MAX_LEN];
    uint8_t  uid_len;
    uint8_t  sak;
    uint16_t atqa;
} nfc_card_t;

esp_err_t nfc_hal_init(void);

/* Liga/desliga a varredura ativa. Apos init() a varredura comeca DESLIGADA;
 * o caller (game logic) decide quando o leitor deve estar buscando cartoes.
 * Enquanto desligado, a task interna fica bloqueada num semaforo — zero I/O. */
esp_err_t nfc_hal_start_scanning(void);
esp_err_t nfc_hal_stop_scanning(void);

/* Bloqueia ate detectar um cartao novo ou expirar timeout. UINT32_MAX espera
 * indefinidamente. So gera evento se a varredura estiver ativa.
 * Mesmo cartao mantido na antena gera UM unico evento; sair e voltar conta
 * como evento novo. start_scanning tambem reseta o tracker de de-dup. */
bool      nfc_hal_wait_card(nfc_card_t *card, uint32_t timeout_ms);

#ifdef __cplusplus
}
#endif
