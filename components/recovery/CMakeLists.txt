idf_component_register(SRCS "recovery.c"
                       INCLUDE_DIRS "include"
                       REQUIRES freertos
                                log
                                esp_tinyusb)
