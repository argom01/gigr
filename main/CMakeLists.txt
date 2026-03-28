idf_component_register(SRCS "gigr.c"
                    INCLUDE_DIRS "."
                    REQUIRES
                        lvgl
                        esp_driver_gpio
                        esp_driver_spi
                        esp_driver_i2c
                        esp_timer
                        freertos
                    )
