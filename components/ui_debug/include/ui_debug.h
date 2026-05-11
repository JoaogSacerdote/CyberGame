#pragma once

#include <stdbool.h>
#include "esp_err.h"
#include "nfc_hal.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Tela TEMPORARIA de validacao dos HALs de input.
 *
 * Mostra ao vivo:
 *  - Estado dos botoes A/B/X/Y (bullet verde = pressionado, cinza = solto)
 *  - Joystick: leitura X/Y numerica + ponto numa caixa de referencia
 *  - NFC: estado scanning (ON/OFF) + UID da ultima carta lida
 *  - Uptime + heap free (DRAM interna + PSRAM)
 *
 * Pre-condicao: hal_bridge_init() ja rodou com sucesso.
 *
 * Cria task interna de refresh (~10 Hz) que poll-a button_hal/joystick_hal/
 * heap_caps. Para NFC, expõe duas APIs que o consumer real do nfc_hal deve
 * chamar a cada evento (evita disputa pelo nfc_hal_wait_card single-consumer).
 *
 * Esta tela existe ate game_logic assumir a UI.
 */
esp_err_t ui_debug_init(void);

/* Thread-safe. Chame quando nfc_hal_wait_card retornar com carta. */
void ui_debug_set_nfc_card(const nfc_card_t *card);

/* Thread-safe. Chame ao ligar/desligar a varredura. */
void ui_debug_set_nfc_scanning(bool on);

#ifdef __cplusplus
}
#endif
