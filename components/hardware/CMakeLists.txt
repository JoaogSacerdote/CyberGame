idf_component_register(SRCS "pmu.c"
                            "button_hal.c"
                       INCLUDE_DIRS "include"
                       REQUIRES esp_driver_gpio
                                esp_driver_spi
                                esp_driver_ledc
                                esp_lcd
                                esp_hw_support
                                esp_psram
                                freertos
                                log)
