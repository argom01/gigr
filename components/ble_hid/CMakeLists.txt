idf_component_register(SRCS "ble_hid.c" "esp_hidd_prf_api.c" "hid_dev.c" "hid_device_le_prf.c"
                    INCLUDE_DIRS "." "./include"
                    PRIV_REQUIRES
                        nvs_flash
                        esp_hid
                        bt
                    )