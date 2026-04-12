#include "ble_hid.h"

#include <string.h>
#include "esp_log.h"

#include "esp_bt.h"
#include "esp_bt_main.h"
#include "esp_bt_device.h"

#include "esp_gap_ble_api.h"
#include "esp_gatts_api.h"
#include "esp_hidd_prf_api.h"
#include "hid_dev.h"
#include "nvs_flash.h"

static const char *TAG = "HID_DEVICE";
static uint16_t s_conn_id   = 0;
static bool     s_connected = false;

// ── GAP advertising data ──────────────────────────────────────────────────────

static esp_ble_adv_data_t adv_data = {
    .set_scan_rsp        = false,
    .include_name        = true,
    .include_txpower     = true,
    .min_interval        = 0x0006,  // 7.5 ms
    .max_interval        = 0x0010,  // 20 ms
    .appearance          = ESP_BLE_APPEARANCE_HID_KEYBOARD,
    .manufacturer_len    = 0,
    .p_manufacturer_data = NULL,
    .service_data_len    = 0,
    .p_service_data      = NULL,
    .service_uuid_len    = 0,
    .p_service_uuid      = NULL,
    .flag = (ESP_BLE_ADV_FLAG_GEN_DISC | ESP_BLE_ADV_FLAG_BREDR_NOT_SPT),
};

static esp_ble_adv_params_t adv_params = {
    .adv_int_min       = 0x20,
    .adv_int_max       = 0x30,
    .adv_type          = ADV_TYPE_IND,
    .own_addr_type     = BLE_ADDR_TYPE_PUBLIC,
    .channel_map       = ADV_CHNL_ALL,
    .adv_filter_policy = ADV_FILTER_ALLOW_SCAN_ANY_CON_ANY,
};

// ── Callbacks ─────────────────────────────────────────────────────────────────

static void gap_event_handler(esp_gap_ble_cb_event_t event,
                               esp_ble_gap_cb_param_t *param)
{
    switch (event) {
    case ESP_GAP_BLE_ADV_DATA_SET_COMPLETE_EVT:
        esp_ble_gap_start_advertising(&adv_params);
        break;
    case ESP_GAP_BLE_SEC_REQ_EVT:
        esp_ble_gap_security_rsp(param->ble_security.ble_req.bd_addr, true);
        break;
    default:
        break;
    }
}

static void hidd_event_callback(esp_hidd_cb_event_t event,
                                 esp_hidd_cb_param_t *param)
{
    ESP_LOGI(TAG, "hidd_event_callback called, event=%d", event);
    switch (event) {
    case ESP_HIDD_EVENT_REG_FINISH:
        if (param->init_finish.state == ESP_HIDD_INIT_OK) {
            ESP_LOGI(TAG, "HID profile registered, gatts_if=%d",
                     param->init_finish.gatts_if);

            esp_ble_gap_config_adv_data(&adv_data);
        }
        break;

    case ESP_BAT_EVENT_REG:
        break;

    case ESP_HIDD_EVENT_BLE_CONNECT:
        ESP_LOGI(TAG, "HID connected, conn_id=%d", param->connect.conn_id);
        s_conn_id   = param->connect.conn_id;
        s_connected = true;
        break;

    case ESP_HIDD_EVENT_BLE_DISCONNECT:
        ESP_LOGI(TAG, "HID disconnected");
        s_connected = false;
        esp_ble_gap_start_advertising(&adv_params);
        break;

    default:
        break;
    }
}

// ── Public API ────────────────────────────────────────────────────────────────

esp_err_t hid_device_init(const char *device_name)
{
    // 1. NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES ||
        ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ESP_ERROR_CHECK(nvs_flash_init());
    }

    // 2. Free unused classic BT memory (S3 = BLE only)
    ESP_ERROR_CHECK(esp_bt_controller_mem_release(ESP_BT_MODE_CLASSIC_BT));

    // 3. BT controller
    esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_bt_controller_init(&bt_cfg));
    ESP_ERROR_CHECK(esp_bt_controller_enable(ESP_BT_MODE_BLE));

    // 4. Bluedroid stack  (use _with_cfg in IDF 5.x)
    esp_bluedroid_config_t bd_cfg = BT_BLUEDROID_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_bluedroid_init_with_cfg(&bd_cfg));
    ESP_ERROR_CHECK(esp_bluedroid_enable());

    // 5. Security params (required for HID bonding)
    esp_ble_auth_req_t auth_req  = ESP_LE_AUTH_BOND;
    esp_ble_io_cap_t   iocap     = ESP_IO_CAP_NONE;
    uint8_t key_size  = 16;
    uint8_t init_key  = ESP_BLE_ENC_KEY_MASK | ESP_BLE_ID_KEY_MASK;
    uint8_t rsp_key   = ESP_BLE_ENC_KEY_MASK | ESP_BLE_ID_KEY_MASK;
    esp_ble_gap_set_security_param(ESP_BLE_SM_AUTHEN_REQ_MODE, &auth_req, sizeof(auth_req));
    esp_ble_gap_set_security_param(ESP_BLE_SM_IOCAP_MODE,      &iocap,    sizeof(iocap));
    esp_ble_gap_set_security_param(ESP_BLE_SM_MAX_KEY_SIZE,    &key_size, sizeof(key_size));
    esp_ble_gap_set_security_param(ESP_BLE_SM_SET_INIT_KEY,    &init_key, sizeof(init_key));
    esp_ble_gap_set_security_param(ESP_BLE_SM_SET_RSP_KEY,     &rsp_key,  sizeof(rsp_key));

    ESP_ERROR_CHECK(esp_hidd_profile_init());   // registers GATTS app & service

    // 6. Register callbacks
    ESP_ERROR_CHECK(esp_ble_gap_register_callback(gap_event_handler));
    ESP_ERROR_CHECK(esp_hidd_register_callbacks(hidd_event_callback));

    esp_ble_gap_set_device_name(device_name ? device_name : "ESP32S3-HID");

    ESP_LOGI(TAG, "HID init done, waiting for REG_FINISH to start advertising");
    return ESP_OK;
}

bool hid_device_connected(void)
{
    return s_connected;
}

void hid_keyboard_send(uint8_t modifier, uint8_t *keycodes, uint8_t num_keys)
{
    if (!s_connected) {
        ESP_LOGW(TAG, "Cannot send keyboard report, not connected");
        return;
    }

    esp_hidd_send_keyboard_value(s_conn_id, modifier, keycodes, num_keys);
}

void hid_keyboard_release(void)
{
    if (!s_connected) return;
    uint8_t empty[1] = {0};
    esp_hidd_send_keyboard_value(s_conn_id, 0, empty, 0);
}

void hid_touchpad_send(int8_t dx, int8_t dy, uint8_t buttons)
{
    if (!s_connected) return;
    esp_hidd_send_mouse_value(s_conn_id, buttons, dx, dy);
}
