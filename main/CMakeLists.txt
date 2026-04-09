idf_component_register(SRCS "mpu6050.c" "gigr.c" "lcd_st7796.c" "ble_hid.c" "esp_hidd_prf_api.c" "hid_dev.c" "hid_device_le_prf.c"
                    INCLUDE_DIRS "." "./include"
                    REQUIRES
                        lvgl
                        esp_driver_gpio
                        esp_driver_spi
                        esp_driver_i2c
                        esp_timer
                        esp_hid
                        bt
                        freertos
                    PRIV_REQUIRES
                        nvs_flash
                    )