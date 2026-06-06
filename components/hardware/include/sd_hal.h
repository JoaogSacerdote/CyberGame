#pragma once

#include <stdbool.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Ponto de montagem do cartao microSD no VFS. Caminhos de arquivo ficam
 * tipo "/sd/assets/player.bin". */
#define SD_MOUNT_POINT  "/sd"

/* Monta o microSD (slot embutido no modulo de display) via SDSPI no
 * SPI2_HOST. PRE-REQUISITO: o barramento SPI2 ja deve estar inicializado
 * (display_hal_init faz isso) — sd_hal NAO chama spi_bus_initialize.
 * Idempotente. Em sucesso, imprime tipo/tamanho do cartao no log. */
esp_err_t sd_hal_init(void);

/* true se o cartao esta montado. */
bool sd_hal_is_mounted(void);

/* Helper de bring-up: lista o conteudo da raiz de /sd no log. Util para
 * confirmar fiacao + cartao logo no boot. */
esp_err_t sd_hal_list_root(void);

/* Teste de integridade: cria um arquivo em /sd, escreve uma string conhecida,
 * fecha, reabre, le de volta e compara. Loga PASSOU/FALHOU. Valida o caminho
 * completo de escrita+leitura do cartao (nao so o mount). Pre-cond: montado. */
esp_err_t sd_hal_selftest(void);

#ifdef __cplusplus
}
#endif
