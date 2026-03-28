idf_component_register(SRCS "mpu6050.c" "gigr.c"
                    INCLUDE_DIRS "." "./include"
                    REQUIRES
                        lvgl
                        esp_driver_gpio
                        esp_driver_spi
                        esp_driver_i2c
                        esp_timer
                        freertos
                    )
