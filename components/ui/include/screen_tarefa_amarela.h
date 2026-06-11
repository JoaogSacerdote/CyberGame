#pragma once
#include <stdbool.h>
#include <stdint.h>

typedef enum {
    TAREFA_AM_CONCLUIDA = 0,
    TAREFA_AM_CANCELADA,
} tarefa_am_result_t;

typedef void (*tarefa_am_cb_t)(tarefa_am_result_t result);

/* srv_id: 0 = servidor esquerda, 1 = servidor direita. */
void screen_tarefa_amarela_build(uint8_t srv_id, tarefa_am_cb_t done_cb);
void screen_tarefa_amarela_destroy(void);
void screen_tarefa_amarela_reset(void);
bool screen_tarefa_amarela_is_open(void);
