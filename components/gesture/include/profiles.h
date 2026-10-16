#ifndef PROFILES_H
#define PROFILES_H

#include <stddef.h>
#include <stdint.h>
#include "gesture.h"
#include "hid_codes.h"

// Pure C: per-host gesture -> HID action tables (see profiles.c).

typedef enum {
    ACT_NONE = 0,
    ACT_KEY,            // modifiers + one key, tapped
    ACT_CONSUMER,       // consumer usage, tapped
    ACT_MOUSE,          // mouse button click
} action_type_t;

typedef struct {
    uint8_t type;       // action_type_t
    uint8_t mods;
    uint8_t key;
    uint16_t usage;     // consumer usage or mouse button
} action_t;

#define KEY(m, k) { .type = ACT_KEY, .mods = (m), .key = (k) }
#define CONS(u)   { .type = ACT_CONSUMER, .usage = (u) }
#define CLICK(b)  { .type = ACT_MOUSE, .usage = (b) }

typedef struct {
    const char *name;
    action_t actions[GESTURE_COUNT];
} host_profile_t;

extern const host_profile_t g_profiles[];
extern const int g_profile_count;

const action_t *profile_action(int profile, int gesture);

// "Ctrl+Win+Left", "Vol+", "-" ...
void action_describe(const action_t *a, char *buf, size_t len);

// '\n'-separated profile names for an LVGL dropdown
const char *profile_dropdown_options(void);

#endif
