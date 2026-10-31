set(MPU6500_DIR "${CMAKE_CURRENT_LIST_DIR}/../mpu6500")

idf_component_register(SRCS "i2c_bus.c" "imu.c" "flex_adc.c" "flex_cal.c" "battery.c"
                            "${MPU6500_DIR}/src/driver_mpu6500.c"
                    INCLUDE_DIRS "include"
                    PRIV_INCLUDE_DIRS "${MPU6500_DIR}/src" "${MPU6500_DIR}/interface"
                    REQUIRES
                        board
                        esp_driver_i2c
                        esp_adc
                        freertos
                    PRIV_REQUIRES
                        esp_driver_gpio
                        esp_timer
                    )

# third-party driver: keep its warnings out of the project build output
set_source_files_properties("${MPU6500_DIR}/src/driver_mpu6500.c" PROPERTIES COMPILE_OPTIONS "-w")
