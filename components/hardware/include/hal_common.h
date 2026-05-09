#pragma once

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Instala o servico ISR do GPIO uma unica vez no ciclo de vida do firmware.
 * Multiplos HALs (button, nfc, ...) chamam esta funcao em vez de
 * gpio_install_isr_service direto, evitando o log "already installed". */
esp_err_t hal_isr_service_install_once(void);

#ifdef __cplusplus
}
#endif
