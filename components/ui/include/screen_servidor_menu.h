#pragma once
#include <stdbool.h>

typedef enum {
    SERVIDOR_MENU_BACKUP = 0,
    SERVIDOR_MENU_WEB,
    SERVIDOR_MENU_CANCELADO,
} servidor_menu_result_t;

typedef void (*servidor_menu_cb_t)(servidor_menu_result_t result);

void screen_servidor_menu_build(servidor_menu_cb_t done_cb);
void screen_servidor_menu_destroy(void);
bool screen_servidor_menu_is_open(void);
