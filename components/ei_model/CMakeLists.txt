# Edge Impulse model component.
#
# Without a model: builds ei_stub.c (no labels, the firmware runs without
# gesture recognition).
#
# With a model: unzip the Edge Impulse "C++ library" export into this folder
# so that edge-impulse-sdk/, model-parameters/ and tflite-model/ sit next to
# this file, then rebuild. The SDK is compiled with the ESP-NN optimised
# kernels for the ESP32-S3.

set(EI_DIR "${CMAKE_CURRENT_LIST_DIR}")

if(EXISTS "${EI_DIR}/edge-impulse-sdk" AND EXISTS "${EI_DIR}/model-parameters")
    file(GLOB_RECURSE EI_SRCS
        "${EI_DIR}/edge-impulse-sdk/tensorflow/*.cc"
        "${EI_DIR}/edge-impulse-sdk/tensorflow/*.c"
        "${EI_DIR}/edge-impulse-sdk/dsp/*.cpp"
        "${EI_DIR}/edge-impulse-sdk/dsp/*.c"
        "${EI_DIR}/edge-impulse-sdk/classifier/*.cpp"
        "${EI_DIR}/edge-impulse-sdk/porting/espressif/*.cpp"
        "${EI_DIR}/edge-impulse-sdk/porting/espressif/ESP-NN/src/*.c"
        "${EI_DIR}/edge-impulse-sdk/porting/espressif/ESP-NN/src/*.S"
        "${EI_DIR}/tflite-model/*.cpp")
    # ARM-only and other-target sources
    list(FILTER EI_SRCS EXCLUDE REGEX ".*/CMSIS/.*")
    list(FILTER EI_SRCS EXCLUDE REGEX ".*(esp32p4|riscv|_esp32\\.S).*")
    list(FILTER EI_SRCS EXCLUDE REGEX ".*/(test|tests|examples)/.*")

    idf_component_register(SRCS "ei_glue.cpp" ${EI_SRCS}
                        INCLUDE_DIRS "include"
                        PRIV_INCLUDE_DIRS
                            "."
                            "edge-impulse-sdk"
                            "edge-impulse-sdk/porting/espressif/ESP-NN/include"
                            "edge-impulse-sdk/porting/espressif/ESP-NN/src/common"
                            "edge-impulse-sdk/third_party/flatbuffers/include"
                            "edge-impulse-sdk/third_party/gemmlowp"
                            "edge-impulse-sdk/third_party/ruy"
                            "model-parameters"
                            "tflite-model"
                        PRIV_REQUIRES esp_timer)

    target_compile_definitions(${COMPONENT_LIB} PRIVATE
        EI_PORTING_ESPRESSIF=1
        EI_CLASSIFIER_TFLITE_ENABLE_ESP_NN=1
        EI_CLASSIFIER_TFLITE_ENABLE_ESP_NN_S3=1
        EIDSP_USE_CMSIS_DSP=0
        EIDSP_QUANTIZE_FILTERBANK=0
        TF_LITE_STATIC_MEMORY
        TF_LITE_DISABLE_X86_NEON)
    target_compile_options(${COMPONENT_LIB} PRIVATE -w -Wno-error)
    message(STATUS "ei_model: using the Edge Impulse export")
else()
    idf_component_register(SRCS "ei_stub.c"
                        INCLUDE_DIRS "include")
    message(STATUS "ei_model: no Edge Impulse export found, building the stub")
endif()
