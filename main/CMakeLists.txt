idf_component_register(SRCS "gigr.c" "sysmon.c" "sensor_task.c"
                    INCLUDE_DIRS "."
                    REQUIRES
                        board
                        appcore
                        display
                        gui
                        sensors
                        fusion
                    PRIV_REQUIRES
                        nvs_flash
                        esp_timer
                        freertos
                    )
