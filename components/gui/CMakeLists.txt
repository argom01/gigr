idf_component_register(SRCS "gui.c" "gui_widgets.c" "scr_home.c" "scr_sensors.c" "scr_imu.c" "scr_flex.c" "scr_system.c" "scr_touchtest.c"
                    INCLUDE_DIRS "include"
                    PRIV_REQUIRES
                        lvgl
                        appcore
                        board
                        display
                        esp_timer
                        freertos
                    )
