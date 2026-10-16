#ifndef BLE_HID_H
#define BLE_HID_H

#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"

// BLE HID-over-GATT device (Bluedroid): mouse (id 1, with wheel and AC pan),
// keyboard (id 2), consumer control (id 3) and the battery service.
// Application code normally goes through hid_tx.h instead of these calls.

esp_err_t hid_device_init(const char *device_name);

bool hid_device_connected(void);
bool hid_device_advertising(void);
bool hid_device_congested(void);
// Address of the connected (or last connected) host
void hid_device_peer(uint8_t bda[6]);

// Raw report senders (call from one task only: hid_tx)
void hid_keyboard_send(uint8_t modifier, const uint8_t *keycodes, uint8_t num_keys);
void hid_keyboard_release(void);
void hid_consumer_send(uint16_t usage);     // 0 = release
void hid_mouse_send(uint8_t buttons, int16_t dx, int16_t dy, int8_t wheel, int8_t pan);

void hid_device_set_battery(uint8_t pct);

// Forget all bonded hosts (they must pair again)
void hid_device_clear_bonds(void);

#endif // BLE_HID_H
