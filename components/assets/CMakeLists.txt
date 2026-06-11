idf_component_register(
    SRCS
        "asset_loader.c"
        "dialog_loader.c"
        "asset_chao.c"
        "asset_icone_vermelho.c"
        "collision/collision_recepcao.c"
        "collision/collision_empresa.c"
    INCLUDE_DIRS "include"
    REQUIRES "lvgl__lvgl" "heap" "log"
    EMBED_FILES
        "chao_rgb565.bin"
        "icone_vermelho_rgb565a8.bin"
)
