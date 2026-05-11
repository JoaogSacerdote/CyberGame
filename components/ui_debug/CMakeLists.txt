idf_component_register(SRCS "ui_debug.c"
                       INCLUDE_DIRS "include"
                       REQUIRES hardware hal_bridge lvgl__lvgl
                       PRIV_REQUIRES esp_timer)
