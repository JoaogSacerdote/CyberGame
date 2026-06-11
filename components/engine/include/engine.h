#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Liga/desliga o JOGADOR-FANTASMA (task de teste que dirige o loop e loga
 * tudo). Controlado pela variavel DEV_TEST_MODE em main/main.c — main chama
 * isto antes de engine_start. Default desligado. */
void engine_set_test_mode(bool enable);

esp_err_t engine_init(void);
esp_err_t engine_start(void);

/* true se o servidor srv (0 ou 1) foi destruido por ataque nesta run. */
bool        engine_server_is_lost(uint8_t srv);

/* Nome do ataque que destruiu srv ("DDoS", "Ransomware"...). "" se nao perdido. */
const char *engine_server_lost_nome(uint8_t srv);

#ifdef __cplusplus
}
#endif
