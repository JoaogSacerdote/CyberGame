#pragma once

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Inicializa o stack USB CDC (TinyUSB). Apos esta chamada o ESP enumera
 * como uma porta serial virtual no PC (alem da porta UART/JTAG existente). */
esp_err_t recovery_init(void);

/* Loop bloqueante: monta frames binarios vindos do CDC, valida CRC e
 * despacha os comandos do protocolo de assets. Nao retorna — chamador
 * segue para shutdown via PWR.
 *
 * Pre-condicao: storage_hal_init() e asset_store_init() ja rodaram.
 *
 * Protocolo completo (frames, comandos, layouts) em recovery_proto.h.
 * Comandos: PING, LIST, PUT_BEGIN/DATA/END/ABORT, GET, ERASE_CAT,
 * FACTORY_RESET, SELFTEST (validacao fisica do NAND sob demanda). */
void recovery_run(void);

#ifdef __cplusplus
}
#endif
