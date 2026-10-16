#ifndef HID_TX_H
#define HID_TX_H

#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"

// Single sender of all HID reports (task on core 0). Other tasks queue key
// and consumer taps; mouse motion is accumulated and flushed at most once per
// BLE connection interval so sub-pixel motion is never lost and the link is
// never flooded. Nothing is queued while no host is connected.

esp_err_t hid_tx_start(void);

// Press, hold GESTURE_KEY_HOLD_MS, release. Up to 6 keys plus modifiers.
void hid_tx_keys(uint8_t mods, const uint8_t *keys, uint8_t n);
void hid_tx_key(uint8_t mods, uint8_t key);
void hid_tx_consumer(uint16_t usage);

// Mouse
void hid_tx_mouse_move(int32_t dx, int32_t dy, int32_t wheel, int32_t pan);
void hid_tx_mouse_buttons(uint8_t buttons);     // held state (drag)
void hid_tx_mouse_click(uint8_t button);        // press + release

#endif
