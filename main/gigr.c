#include "app_state.h"
#include "board_io.h"
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

    ESP_ERROR_CHECK(sysmon_start());

    ESP_LOGI(TAG, "init done, mode %s", app_mode_name(app_mode_get()));
}
