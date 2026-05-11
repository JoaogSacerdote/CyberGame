idf_component_register(SRCS "asset_store.c"
                       INCLUDE_DIRS "include"
                       REQUIRES hardware
                       PRIV_REQUIRES esp_rom log)
