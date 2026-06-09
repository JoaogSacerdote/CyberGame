#pragma once
#include <stdbool.h>

typedef enum {
    TAREFA_VD_CONCLUIDA = 0,
    TAREFA_VD_CANCELADA,
} tarefa_vd_result_t;

typedef void (*tarefa_vd_cb_t)(tarefa_vd_result_t result);

void screen_tarefa_verde_build(tarefa_vd_cb_t done_cb);
void screen_tarefa_verde_destroy(void);
bool screen_tarefa_verde_is_open(void);
