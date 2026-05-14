idf_component_register(SRCS "recovery.c"
                       INCLUDE_DIRS "include"
                       REQUIRES freertos
                                log
                                esp_tinyusb
                       PRIV_REQUIRES asset_store
                                     hardware
                                     esp_rom)
