idf_component_register(
    SRCS "engine.c"
         "defense_matrix.c"
         "threat.c"
    INCLUDE_DIRS "include"
    REQUIRES
        "fsm"
        "hardware"
        "freertos"
        "log"
    PRIV_REQUIRES
        "ui"
        "entity"         # entity_pool + y_sort + debug_overlay
        "esp_system"     # esp_task_wdt
)
