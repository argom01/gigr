#include "ble_hid.h"

#include <stdlib.h>
#include <string.h>
#include "esp_log.h"

#include "esp_bt.h"
#include "esp_bt_device.h"
#include "esp_bt_main.h"

#include "esp_gap_ble_api.h"
#include "esp_gatts_api.h"
#include "esp_hidd_prf_api.h"
#include "hid_dev.h"

static const char *TAG = "HID_DEVICE";
static uint16_t s_conn_id = 0;
static volatile bool s_connected = false;
static volatile bool s_advertising = false;
static uint8_t s_peer[6];

// ── GAP advertising data ──────────────────────────────────────────────────────

static uint8_t s_hid_service_uuid128[16] = {
    // 0x1812 (HID) in the Bluetooth base UUID
    0xfb, 0x34, 0x9b, 0x5f, 0x80, 0x00, 0x00, 0x80, 0x00, 0x10, 0x00, 0x00, 0x12, 0x18, 0x00, 0x00,
};

static esp_ble_adv_data_t adv_data = {
    .set_scan_rsp        = false,
    .include_name        = true,
    .include_txpower     = false,
    .min_interval        = 0x0006,  // 7.5 ms
    .max_interval        = 0x000C,  // 15 ms
    .appearance          = ESP_BLE_APPEARANCE_GENERIC_HID,
    .manufacturer_len    = 0,
    .p_manufacturer_data = NULL,
    .service_data_len    = 0,
    .p_service_data      = NULL,
    .service_uuid_len    = sizeof(s_hid_service_uuid128),
    .p_service_uuid      = s_hid_service_uuid128,
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

static void start_advertising(void)
{
    esp_ble_gap_start_advertising(&adv_params);
}

// ── Callbacks ─────────────────────────────────────────────────────────────────

static void gap_event_handler(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param)
{
    switch (event) {
    case ESP_GAP_BLE_ADV_DATA_SET_COMPLETE_EVT:
        start_advertising();
        break;
    case ESP_GAP_BLE_ADV_START_COMPLETE_EVT:
        s_advertising = param->adv_start_cmpl.status == ESP_BT_STATUS_SUCCESS;
        break;
    case ESP_GAP_BLE_ADV_STOP_COMPLETE_EVT:
        s_advertising = false;
        break;
    case ESP_GAP_BLE_SEC_REQ_EVT:
        esp_ble_gap_security_rsp(param->ble_security.ble_req.bd_addr, true);
        break;
    case ESP_GAP_BLE_AUTH_CMPL_EVT:
        if (param->ble_security.auth_cmpl.success) {
            ESP_LOGI(TAG, "paired / encrypted");
        } else {
            ESP_LOGW(TAG, "pairing failed, reason 0x%x", param->ble_security.auth_cmpl.fail_reason);
        }
        break;
    case ESP_GAP_BLE_UPDATE_CONN_PARAMS_EVT:
        ESP_LOGI(TAG, "conn params: interval %.2f ms, latency %d, timeout %d ms",
                 param->update_conn_params.conn_int * 1.25, param->update_conn_params.latency,
                 param->update_conn_params.timeout * 10);
        break;
    default:
        break;
    }
}

static void request_fast_connection(const esp_bd_addr_t bda)
{
    // 7.5-15 ms interval, no slave latency: low mouse latency
    esp_ble_conn_update_params_t p = {
        .min_int = 0x06,
        .max_int = 0x0C,
        .latency = 0,
        .timeout = 200,     // x10 ms
    };
    memcpy(p.bda, bda, sizeof(esp_bd_addr_t));
    esp_ble_gap_update_conn_params(&p);
}

static void hidd_event_callback(esp_hidd_cb_event_t event, esp_hidd_cb_param_t *param)
{
    switch (event) {
    case ESP_HIDD_EVENT_REG_FINISH:
        if (param->init_finish.state == ESP_HIDD_INIT_OK) {
            ESP_LOGI(TAG, "HID profile registered");
            esp_ble_gap_config_adv_data(&adv_data);
        }
        break;

    case ESP_BAT_EVENT_REG:
        break;

    case ESP_HIDD_EVENT_BLE_CONNECT:
        ESP_LOGI(TAG, "connected, conn_id=%d", param->connect.conn_id);
        s_conn_id = param->connect.conn_id;
        memcpy(s_peer, param->connect.remote_bda, sizeof(s_peer));
        s_connected = true;
        s_advertising = false;
        request_fast_connection(param->connect.remote_bda);
        break;

    case ESP_HIDD_EVENT_BLE_DISCONNECT:
        ESP_LOGI(TAG, "disconnected");
        s_connected = false;
        start_advertising();
        break;

    default:
        break;
    }
}

// ── Public API ────────────────────────────────────────────────────────────────

esp_err_t hid_device_init(const char *device_name)
{
    // NVS must already be initialised (bonding keys live there)

    // Free unused classic BT memory (S3 = BLE only)
    ESP_ERROR_CHECK(esp_bt_controller_mem_release(ESP_BT_MODE_CLASSIC_BT));

    esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_bt_controller_init(&bt_cfg));
    ESP_ERROR_CHECK(esp_bt_controller_enable(ESP_BT_MODE_BLE));

    esp_bluedroid_config_t bd_cfg = BT_BLUEDROID_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_bluedroid_init_with_cfg(&bd_cfg));
    ESP_ERROR_CHECK(esp_bluedroid_enable());

    // Security: bonding, "just works" (no display/keyboard on the glove side)
    esp_ble_auth_req_t auth_req = ESP_LE_AUTH_BOND;
    esp_ble_io_cap_t iocap = ESP_IO_CAP_NONE;
    uint8_t key_size = 16;
    uint8_t init_key = ESP_BLE_ENC_KEY_MASK | ESP_BLE_ID_KEY_MASK;
    uint8_t rsp_key = ESP_BLE_ENC_KEY_MASK | ESP_BLE_ID_KEY_MASK;
    esp_ble_gap_set_security_param(ESP_BLE_SM_AUTHEN_REQ_MODE, &auth_req, sizeof(auth_req));
    esp_ble_gap_set_security_param(ESP_BLE_SM_IOCAP_MODE, &iocap, sizeof(iocap));
    esp_ble_gap_set_security_param(ESP_BLE_SM_MAX_KEY_SIZE, &key_size, sizeof(key_size));
    esp_ble_gap_set_security_param(ESP_BLE_SM_SET_INIT_KEY, &init_key, sizeof(init_key));
    esp_ble_gap_set_security_param(ESP_BLE_SM_SET_RSP_KEY, &rsp_key, sizeof(rsp_key));

    ESP_ERROR_CHECK(esp_hidd_profile_init());

    ESP_ERROR_CHECK(esp_ble_gap_register_callback(gap_event_handler));
    ESP_ERROR_CHECK(esp_hidd_register_callbacks(hidd_event_callback));

    esp_ble_gap_set_device_name(device_name ? device_name : "ESP32S3-HID");

    ESP_LOGI(TAG, "HID init done, advertising starts after registration");
    return ESP_OK;
}

bool hid_device_connected(void)
{
    return s_connected;
}

bool hid_device_advertising(void)
{
    return s_advertising;
}

bool hid_device_congested(void)
{
    return hidd_is_congested();
}

void hid_device_peer(uint8_t bda[6])
{
    memcpy(bda, s_peer, sizeof(s_peer));
}

void hid_keyboard_send(uint8_t modifier, const uint8_t *keycodes, uint8_t num_keys)
{
    if (!s_connected) {
        return;
    }
    uint8_t keys[6] = { 0 };
    if (num_keys > 6) {
        num_keys = 6;
    }
    if (keycodes && num_keys) {
        memcpy(keys, keycodes, num_keys);
    }
    esp_hidd_send_keyboard_value(s_conn_id, modifier, keys, num_keys);
}

void hid_keyboard_release(void)
{
    if (!s_connected) {
        return;
    }
    uint8_t empty[1] = { 0 };
    esp_hidd_send_keyboard_value(s_conn_id, 0, empty, 0);
}

void hid_consumer_send(uint16_t usage)
{
    if (!s_connected) {
        return;
    }
    esp_hidd_send_consumer_value(s_conn_id, usage, usage != 0);
}

void hid_mouse_send(uint8_t buttons, int16_t dx, int16_t dy, int8_t wheel, int8_t pan)
{
    if (!s_connected) {
        return;
    }
    esp_hidd_send_mouse_value(s_conn_id, buttons, dx, dy, wheel, pan);
}

void hid_device_set_battery(uint8_t pct)
{
    hidd_set_battery_level(s_conn_id, s_connected, pct);
}

void hid_device_clear_bonds(void)
{
    const int n = esp_ble_get_bond_device_num();
    if (n <= 0) {
        return;
    }
    esp_ble_bond_dev_t *list = calloc((size_t)n, sizeof(esp_ble_bond_dev_t));
    if (!list) {
        return;
    }
    int count = n;
    esp_ble_get_bond_device_list(&count, list);
    for (int i = 0; i < count; i++) {
        esp_ble_remove_bond_device(list[i].bd_addr);
    }
    free(list);
    ESP_LOGI(TAG, "removed %d bonded hosts", count);
}
