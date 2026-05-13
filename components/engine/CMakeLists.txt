idf_component_register(
    SRCS "engine.c"
    INCLUDE_DIRS "include"
    REQUIRES
        "fsm"
        "hardware"
        "freertos"
        "log"
    PRIV_REQUIRES
        "ui"
)
