#pragma once

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Inicializa o stack USB CDC (TinyUSB). Apos esta chamada o ESP enumera
 * como uma porta serial virtual no PC (alem da porta UART/JTAG existente). */
esp_err_t recovery_init(void);

/* Loop bloqueante: le linhas terminadas em '\n' do CDC, parseia comandos
 * e responde. Nao retorna — chamador segue para idle apos saida via QUIT
 * (futuro) ou shutdown via PWR.
 *
 * Comandos B1: PING -> PONG. Tudo o mais retorna UNKNOWN. */
void recovery_run(void);

#ifdef __cplusplus
}
#endif
