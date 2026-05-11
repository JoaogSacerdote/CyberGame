idf_component_register(SRCS "hal_bridge.c"
                       INCLUDE_DIRS "include"
                       REQUIRES hardware lvgl__lvgl
                       PRIV_REQUIRES esp_timer)
