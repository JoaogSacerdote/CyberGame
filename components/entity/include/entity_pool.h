#pragma once

#include <stddef.h>
#include "entity.h"
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Capacidade fixa em BSS (~2 KB para 64 entities). Suficiente para
 * sala com player + ate ~5 NPCs + ate ~20 moveis + triggers. Aumentar
 * se uma sala futura passar do limite. */
#define ENTITY_POOL_CAPACITY  64

/* Inicializa pool. Idempotente — chamadas subsequentes resetam estado.
 * Nao toca em lv_objs (caller pode reinicializar UI primeiro). */
esp_err_t entity_pool_init(void);

/* Libera TODAS as entities vivas. Deleta lv_objs associados (chame
 * sob lv_lock se tiver outra task usando LVGL). */
void entity_pool_clear(void);

/* Aloca slot vazio. Retorna entity zerada com id unico ja preenchido.
 * Caller preenche os demais campos. NULL se pool cheio. */
entity_t *entity_pool_alloc(void);

/* Libera entity especifica. Deleta lv_obj se nao for NULL. */
void entity_pool_free(entity_t *e);

/* Quantidade de entities vivas. */
size_t entity_pool_count(void);

/* Itera pelas vivas. i = 0..count-1. Retorna NULL se i invalido.
 * Ordem entre frames eh estavel se nao houver alloc/free no meio. */
entity_t *entity_pool_at(size_t i);

#ifdef __cplusplus
}
#endif
