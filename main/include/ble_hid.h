#pragma once
#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"

// Initialize BT stack and start advertising as a BLE HID device
esp_err_t hid_device_init(const char *device_name);

// Returns true if a host is currently connected
bool hid_device_connected(void);

// Keyboard: modifier = bitmask (e.g. 0x02 = L-Shift), keycodes = up to 6 HID keycodes
void hid_keyboard_send(uint8_t modifier, uint8_t *keycodes, uint8_t num_keys);

// Keyboard: release all keys
void hid_keyboard_release(void);

// Touchpad: dx/dy = relative movement, buttons = bitmask bit0=left,bit1=right,bit2=middle
void hid_touchpad_send(int8_t dx, int8_t dy, uint8_t buttons);