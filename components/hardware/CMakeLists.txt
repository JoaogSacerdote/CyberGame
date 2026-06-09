idf_component_register(SRCS "pmu.c"
                            "button_hal.c"
                            "joystick_hal.c"
                            "nfc_hal.c"
                            "hal_common.c"
                            "display_hal.c"
                            "ws2812_hal.c"
                            "buzzer_hal.c"
                            "sd_hal.c"
                       INCLUDE_DIRS "include"
                       REQUIRES calibracao
                                esp_driver_gpio
                                esp_driver_spi
                                esp_driver_ledc
                                esp_driver_i2c
                                esp_driver_sdspi
                                esp_adc
                                esp_lcd
                                esp_hw_support
                                esp_psram
                                esp_timer
                                espressif__led_strip
                                fatfs
                                sdmmc
                                freertos
                                log)
