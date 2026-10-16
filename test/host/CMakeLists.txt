# Host-side unit tests for the hardware independent modules.
#   cmake -S test/host -B build-host && cmake --build build-host && ctest --test-dir build-host
cmake_minimum_required(VERSION 3.16)
project(gigr_host_tests C)

set(CMAKE_C_STANDARD 11)
set(ROOT ${CMAKE_CURRENT_LIST_DIR}/../..)
add_compile_options(-Wall -Wextra -Wno-unused-parameter -g)

include_directories(
    ${ROOT}/components/board/include
    ${ROOT}/components/fusion/include
    ${ROOT}/components/sensors/include
    ${CMAKE_CURRENT_LIST_DIR}
)

add_library(fusion STATIC
    ${ROOT}/components/fusion/quat.c
    ${ROOT}/components/fusion/mahony.c
    ${ROOT}/components/fusion/gyro_bias.c
    ${ROOT}/components/fusion/one_euro.c
)
target_link_libraries(fusion m)

add_library(sensors_pure STATIC
    ${ROOT}/components/sensors/flex_cal.c
    ${ROOT}/components/sensors/battery.c
)
target_link_libraries(sensors_pure m)

enable_testing()

function(host_test name)
    add_executable(${name} ${name}.c)
    target_link_libraries(${name} ${ARGN} m)
    add_test(NAME ${name} COMMAND ${name})
endfunction()

host_test(test_fusion fusion)
host_test(test_flex sensors_pure)
host_test(test_battery sensors_pure)
