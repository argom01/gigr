#ifndef BLE_HID_H
#define BLE_HID_H

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"

esp_err_t hid_device_init(const char *device_name);

// Returns true if a host is currently connected
bool hid_device_connected(void);

void hid_keyboard_send(uint8_t modifier, uint8_t *keycodes, uint8_t num_keys);

void hid_keyboard_release(void);

void hid_touchpad_send(int8_t dx, int8_t dy, uint8_t buttons);

#endif // BLE_HID_H