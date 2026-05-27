idf_component_register(
    SRCS
        "entity_pool.c"
        "movement.c"
        "y_sort.c"
        "debug_overlay.c"
    INCLUDE_DIRS "include"
    REQUIRES
        "lvgl__lvgl"
        "log"
)
