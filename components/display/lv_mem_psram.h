#ifndef LV_MEM_PSRAM_H
#define LV_MEM_PSRAM_H

// Included by LVGL's builtin allocator through LV_MEM_POOL_INCLUDE (set on the
// lvgl target in this component's CMakeLists.txt). The whole LV_MEM_SIZE pool
// is allocated from PSRAM at lv_init(), so LVGL objects stay out of the
// internal RAM that Bluetooth needs.

#include "esp_heap_caps.h"

#define LV_MEM_POOL_ALLOC(size) heap_caps_malloc((size), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT)

#endif
