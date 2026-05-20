idf_component_register(
    SRCS
        "asset_loader.c"
        "dialog_loader.c"
        "collision/collision_recepcao.c"
        "collision/collision_empresa.c"
    INCLUDE_DIRS "include"
    REQUIRES "lvgl__lvgl" "asset_store"
)
