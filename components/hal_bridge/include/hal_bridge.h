#pragma once

#include "esp_err.h"
#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Inicializa o LVGL e conecta-o ao display_hal.
 *
 * Pre-condicao: display_hal_init() ja foi chamado com sucesso.
 *
 * Apos retornar com ESP_OK:
 *  - LVGL esta inicializado e a tela ativa pode receber widgets
 *  - Buffers de strip estao alocados (~30 KB cada, double buffer)
 *  - Tick periodico de 5 ms esta rodando via esp_timer
 *  - Task "lvgl" esta processando lv_timer_handler() em loop
 *
 * Chamadas para a API do LVGL fora desse modulo devem ser envolvidas em
 * lv_lock() / lv_unlock().
 */
esp_err_t hal_bridge_init(void);

/**
 * Retorna o handle do display LVGL criado pelo hal_bridge_init.
 * Util para customizacoes pontuais (rotacao, scaling, etc).
 * NULL se hal_bridge_init ainda nao rodou.
 */
lv_display_t *hal_bridge_get_display(void);

#ifdef __cplusplus
}
#endif
