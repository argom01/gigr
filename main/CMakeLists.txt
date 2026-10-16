idf_component_register(SRCS "gigr.c" "sysmon.c"
                    INCLUDE_DIRS "."
                    REQUIRES
                        board
                        appcore
                    PRIV_REQUIRES
                        nvs_flash
                        esp_timer
                        freertos
                    )
