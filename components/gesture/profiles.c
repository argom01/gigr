#include "profiles.h"

#include <stdio.h>
#include <string.h>

// ─────────────────────────────────────────────────────────────────────────────
//  HOST PROFILES
//  One table per operating system / device. Pick the active one in the
//  Gesture tab (optionally remembered per paired host). Gestures without an
//  entry do nothing. Add a profile by appending another block.
// ─────────────────────────────────────────────────────────────────────────────

const host_profile_t g_profiles[] = {
    {
        .name = "Windows",
        .actions = {
            [G_SWIPE_LEFT]  = KEY(MOD_LCTRL | MOD_LGUI, KC_LEFT),     // previous virtual desktop
            [G_SWIPE_RIGHT] = KEY(MOD_LCTRL | MOD_LGUI, KC_RIGHT),    // next virtual desktop
            [G_SWIPE_UP]    = KEY(MOD_LGUI, KC_TAB),                  // task view
            [G_SWIPE_DOWN]  = KEY(MOD_LGUI, KC_D),                    // show desktop
            [G_FIST]        = CONS(CC_PLAY_PAUSE),
            [G_V_SIGN]      = KEY(MOD_LGUI, KC_PSCREEN),              // screenshot
            [G_THUMBS_UP]   = KEY(MOD_NONE, KC_ENTER),
        },
    },
    {
        .name = "macOS",
        .actions = {
            [G_SWIPE_LEFT]  = KEY(MOD_LCTRL, KC_LEFT),                // previous Space
            [G_SWIPE_RIGHT] = KEY(MOD_LCTRL, KC_RIGHT),               // next Space
            [G_SWIPE_UP]    = KEY(MOD_LCTRL, KC_UP),                  // Mission Control
            [G_SWIPE_DOWN]  = KEY(MOD_LGUI, KC_F3),                   // show desktop
            [G_FIST]        = CONS(CC_PLAY_PAUSE),
            [G_V_SIGN]      = KEY(MOD_LGUI | MOD_LSHIFT, KC_3),       // screenshot
            [G_THUMBS_UP]   = KEY(MOD_NONE, KC_ENTER),
        },
    },
    {
        .name = "Linux (GNOME)",
        .actions = {
            [G_SWIPE_LEFT]  = KEY(MOD_LGUI, KC_PGUP),                 // workspace up/left
            [G_SWIPE_RIGHT] = KEY(MOD_LGUI, KC_PGDOWN),               // workspace down/right
            [G_SWIPE_UP]    = KEY(MOD_LGUI, KC_NONE),                 // activities overview
            [G_SWIPE_DOWN]  = KEY(MOD_LGUI, KC_D),                    // hide windows
            [G_FIST]        = CONS(CC_PLAY_PAUSE),
            [G_V_SIGN]      = KEY(MOD_NONE, KC_PSCREEN),
            [G_THUMBS_UP]   = KEY(MOD_NONE, KC_ENTER),
        },
    },
    {
        .name = "Android",
        .actions = {
            [G_SWIPE_LEFT]  = KEY(MOD_LALT | MOD_LSHIFT, KC_TAB),     // previous app
            [G_SWIPE_RIGHT] = KEY(MOD_LALT, KC_TAB),                  // next app
            [G_SWIPE_UP]    = KEY(MOD_LGUI, KC_TAB),                  // recents
            [G_SWIPE_DOWN]  = CONS(CC_AC_HOME),
            [G_FIST]        = CONS(CC_PLAY_PAUSE),
            [G_V_SIGN]      = CONS(CC_AC_BACK),
            [G_THUMBS_UP]   = KEY(MOD_NONE, KC_ENTER),
        },
    },
    {
        .name = "iOS / iPadOS",
        .actions = {
            [G_SWIPE_LEFT]  = KEY(MOD_LGUI | MOD_LSHIFT, KC_TAB),     // previous app
            [G_SWIPE_RIGHT] = KEY(MOD_LGUI, KC_TAB),                  // app switcher
            [G_SWIPE_UP]    = KEY(MOD_LGUI, KC_SPACE),                // search
            [G_SWIPE_DOWN]  = KEY(MOD_LGUI, KC_H),                    // home
            [G_FIST]        = CONS(CC_PLAY_PAUSE),
            [G_V_SIGN]      = KEY(MOD_NONE, KC_ESC),
            [G_THUMBS_UP]   = KEY(MOD_NONE, KC_ENTER),
        },
    },
    {
        .name = "Media only",
        .actions = {
            [G_SWIPE_LEFT]  = CONS(CC_PREV_TRACK),
            [G_SWIPE_RIGHT] = CONS(CC_NEXT_TRACK),
            [G_SWIPE_UP]    = CONS(CC_VOL_UP),
            [G_SWIPE_DOWN]  = CONS(CC_VOL_DOWN),
            [G_FIST]        = CONS(CC_PLAY_PAUSE),
            [G_V_SIGN]      = CONS(CC_MUTE),
            [G_THUMBS_UP]   = CONS(CC_NEXT_TRACK),
        },
    },
};

const int g_profile_count = sizeof(g_profiles) / sizeof(g_profiles[0]);

const action_t *profile_action(int profile, int gesture)
{
    static const action_t none = { 0 };
    if (profile < 0 || profile >= g_profile_count || gesture < 0 || gesture >= GESTURE_COUNT) {
        return &none;
    }
    return &g_profiles[profile].actions[gesture];
}

// ── Descriptions ─────────────────────────────────────────────────────────────

static const char *key_name(uint8_t k)
{
    static char buf[8];
    if (k >= KC_A && k <= KC_Z) {
        snprintf(buf, sizeof(buf), "%c", 'A' + (k - KC_A));
        return buf;
    }
    if (k >= KC_1 && k <= KC_9) {
        snprintf(buf, sizeof(buf), "%c", '1' + (k - KC_1));
        return buf;
    }
    if (k >= KC_F1 && k <= KC_F12) {
        snprintf(buf, sizeof(buf), "F%d", 1 + (k - KC_F1));
        return buf;
    }
    switch (k) {
    case KC_0: return "0";
    case KC_ENTER: return "Enter";
    case KC_ESC: return "Esc";
    case KC_BSPACE: return "Backspace";
    case KC_TAB: return "Tab";
    case KC_SPACE: return "Space";
    case KC_PSCREEN: return "PrtSc";
    case KC_HOME: return "Home";
    case KC_END: return "End";
    case KC_PGUP: return "PgUp";
    case KC_PGDOWN: return "PgDn";
    case KC_DELETE: return "Del";
    case KC_LEFT: return "Left";
    case KC_RIGHT: return "Right";
    case KC_UP: return "Up";
    case KC_DOWN: return "Down";
    default:
        snprintf(buf, sizeof(buf), "0x%02X", k);
        return buf;
    }
}

static const char *consumer_name(uint16_t u)
{
    switch (u) {
    case CC_PLAY_PAUSE: return "Play/Pause";
    case CC_NEXT_TRACK: return "Next track";
    case CC_PREV_TRACK: return "Prev track";
    case CC_STOP: return "Stop";
    case CC_MUTE: return "Mute";
    case CC_VOL_UP: return "Vol+";
    case CC_VOL_DOWN: return "Vol-";
    case CC_AC_HOME: return "Home (AC)";
    case CC_AC_BACK: return "Back (AC)";
    case CC_AC_SEARCH: return "Search (AC)";
    case CC_BRIGHT_UP: return "Brightness+";
    case CC_BRIGHT_DOWN: return "Brightness-";
    default: return "Consumer";
    }
}

void action_describe(const action_t *a, char *buf, size_t len)
{
    switch (a->type) {
    case ACT_KEY: {
        size_t n = 0;
        buf[0] = '\0';
        static const struct {
            uint8_t mask;
            const char *name;
        } mods[] = {
            { MOD_LCTRL | MOD_RCTRL, "Ctrl" },
            { MOD_LALT | MOD_RALT, "Alt" },
            { MOD_LSHIFT | MOD_RSHIFT, "Shift" },
            { MOD_LGUI | MOD_RGUI, "Win/Cmd" },
        };
        for (size_t i = 0; i < sizeof(mods) / sizeof(mods[0]); i++) {
            if (a->mods & mods[i].mask) {
                n += (size_t)snprintf(buf + n, len > n ? len - n : 0, "%s%s", n ? "+" : "", mods[i].name);
            }
        }
        if (a->key != KC_NONE) {
            snprintf(buf + n, len > n ? len - n : 0, "%s%s", n ? "+" : "", key_name(a->key));
        }
        break;
    }
    case ACT_CONSUMER:
        snprintf(buf, len, "%s", consumer_name(a->usage));
        break;
    case ACT_MOUSE:
        snprintf(buf, len, "%s click", a->usage == MB_RIGHT ? "Right" : a->usage == MB_MIDDLE ? "Middle" : "Left");
        break;
    default:
        snprintf(buf, len, "-");
        break;
    }
}

const char *profile_dropdown_options(void)
{
    static char buf[256];
    if (buf[0] == '\0') {
        size_t n = 0;
        for (int i = 0; i < g_profile_count && n < sizeof(buf); i++) {
            n += (size_t)snprintf(buf + n, sizeof(buf) - n, "%s%s", i ? "\n" : "", g_profiles[i].name);
        }
    }
    return buf;
}
