idf_component_register(SRCS "gesture_defs.c" "postproc.c" "profiles.c" "frame_ring.c" "gesture_task.c" "capture.c"
                    INCLUDE_DIRS "include"
                    REQUIRES
                        ble_hid
                    PRIV_REQUIRES
                        appcore
                        board
                        ei_model
                        esp_timer
                        freertos
                    )
