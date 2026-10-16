#include "app_state.h"
#include "ble_hid.h"
#include "board_config.h"
#include "capture.h"
#include "hid_tx.h"
#include "board_io.h"
#include "display.h"
#include "flex_adc.h"
#include "gesture_task.h"
#include "gui.h"
#include "i2c_bus.h"
#include "imu.h"
#include "sensor_task.h"
#include "touch.h"
#include "touchpad_mode.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "settings.h"
#include "sysmon.h"

static const char *TAG = "gigr";

static void init_nvs(void)
{
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
}

void app_main(void)
{
    ESP_LOGI(TAG, "GIGR glove starting");

    init_nvs();
    app_state_init();
    settings_init();
    ESP_ERROR_CHECK(board_io_init());

    // Sensors: a missing device is logged and marked, the rest keeps working
    if (i2c_bus_init() != ESP_OK) {
        ESP_LOGE(TAG, "I2C bus init failed");
    }
    app_hw_set(HW_IMU, imu_init() == ESP_OK ? HW_OK : HW_MISSING);
    app_hw_set(HW_FLEX, flex_adc_init() == ESP_OK ? HW_OK : HW_MISSING);

    // Display: no reliable presence detection on a write-only SPI panel, so
    // only bus/driver errors mark it missing.
    if (display_init() == ESP_OK) {
        app_hw_set(HW_DISPLAY, HW_OK);
        // the touch input device must exist before the GUI task starts
        app_hw_set(HW_TOUCH, touch_init() == ESP_OK ? HW_OK : HW_MISSING);
        gui_init();
        display_start();
        if (app_hw_ok(HW_TOUCH)) {
            touchpad_mode_init();
            touch_start();
        }
    } else {
        ESP_LOGE(TAG, "display init failed");
        app_hw_set(HW_DISPLAY, HW_MISSING);
        app_hw_set(HW_TOUCH, HW_MISSING);
    }

    // Bluetooth LE HID (on-chip, so failures are fatal configuration errors)
    if (hid_device_init(GLOVE_DEVICE_NAME) == ESP_OK && hid_tx_start() == ESP_OK) {
        app_hw_set(HW_BLE, HW_OK);
    } else {
        app_hw_set(HW_BLE, HW_MISSING);
    }

    ESP_ERROR_CHECK(gesture_start());
    ESP_ERROR_CHECK(capture_init());
    ESP_ERROR_CHECK(sensor_task_start());
    ESP_ERROR_CHECK(sysmon_start());

    ESP_LOGI(TAG, "init done, mode %s", app_mode_name(app_mode_get()));
}
