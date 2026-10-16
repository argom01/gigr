idf_component_register(SRCS "gigr.c" "sysmon.c" "sensor_task.c" "touchpad_mode.c"
                    INCLUDE_DIRS "."
                    REQUIRES
                        board
                        appcore
                        display
                        gui
                        sensors
                        fusion
                        ble_hid
                        airmouse
                        gesture
                    PRIV_REQUIRES
                        nvs_flash
                        esp_timer
                        freertos
                    )
