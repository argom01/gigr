# Pure C (no ESP-IDF headers) so test/host can build the same sources
idf_component_register(SRCS "airmouse.c"
                    INCLUDE_DIRS "include"
                    REQUIRES fusion board)
