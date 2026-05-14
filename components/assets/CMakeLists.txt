idf_component_register(
    SRCS
        "generated/img_recepcao.c"
        "generated/img_empresa.c"
        "generated/img_player.c"
        "collision/collision_recepcao.c"
        "collision/collision_empresa.c"
    INCLUDE_DIRS "include"
    REQUIRES "lvgl__lvgl"
)
