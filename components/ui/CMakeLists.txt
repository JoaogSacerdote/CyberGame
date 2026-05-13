idf_component_register(
    SRCS
        "ui_router.c"
        "screen_splash.c"
        "screen_menu.c"
        "screen_placeholder.c"
        "screen_pause.c"
    INCLUDE_DIRS "include"
    REQUIRES
        "hardware"
        "hal_bridge"
        "lvgl__lvgl"
        "fsm"
    PRIV_REQUIRES
        "esp_timer"
)
