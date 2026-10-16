idf_component_register(SRCS "app_state.c" "app_req.c" "settings.c"
                    INCLUDE_DIRS "include"
                    REQUIRES
                        board
                    PRIV_REQUIRES
                        nvs_flash
                        esp_timer
                    )
