# Pure C (no ESP-IDF headers) so test/host can build the same sources
idf_component_register(SRCS "quat.c" "mahony.c" "gyro_bias.c" "one_euro.c"
                    INCLUDE_DIRS "include")
