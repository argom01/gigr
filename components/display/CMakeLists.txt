idf_component_register(SRCS "display.c" "touch.c" "touchpad.c"
                    INCLUDE_DIRS "include"
                    REQUIRES
                        lvgl
                        esp_lcd
                    PRIV_REQUIRES
                        board
                        appcore
                        esp_driver_spi
                        esp_driver_gpio
                        esp_timer
                        espressif__esp_lcd_st7796
                        espressif__esp_lcd_touch_gt911
                        sensors
                    )
