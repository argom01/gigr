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

# LVGL heap in PSRAM. lv_mem_add_pool() can't be used for this: TLSF caps
# every pool at LV_MEM_SIZE, so the pool size itself is set in sdkconfig
# (CONFIG_LV_MEM_SIZE) and allocated from PSRAM by lv_mem_psram.h.
idf_component_get_property(lvgl_lib lvgl COMPONENT_LIB)
target_compile_definitions(${lvgl_lib} PRIVATE
    "LV_MEM_POOL_INCLUDE=\"${CMAKE_CURRENT_LIST_DIR}/lv_mem_psram.h\"")
