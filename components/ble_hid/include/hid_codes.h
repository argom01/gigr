#ifndef HID_CODES_H
#define HID_CODES_H

// Pure header (no ESP-IDF): HID usages used by the gesture profiles, the
// touchpad and the air mouse. Keyboard usages are limited to 0x00..0x65 by
// the report descriptor.

// ── Keyboard modifiers ───────────────────────────────────────────────────────
#define MOD_NONE    0x00
#define MOD_LCTRL   0x01
#define MOD_LSHIFT  0x02
#define MOD_LALT    0x04
#define MOD_LGUI    0x08    // Windows / Command / Super
#define MOD_RCTRL   0x10
#define MOD_RSHIFT  0x20
#define MOD_RALT    0x40
#define MOD_RGUI    0x80

// ── Keyboard usages ──────────────────────────────────────────────────────────
#define KC_NONE     0x00
#define KC_A        0x04
#define KC_B        0x05
#define KC_C        0x06
#define KC_D        0x07
#define KC_E        0x08
#define KC_F        0x09
#define KC_G        0x0A
#define KC_H        0x0B
#define KC_I        0x0C
#define KC_J        0x0D
#define KC_K        0x0E
#define KC_L        0x0F
#define KC_M        0x10
#define KC_N        0x11
#define KC_O        0x12
#define KC_P        0x13
#define KC_Q        0x14
#define KC_R        0x15
#define KC_S        0x16
#define KC_T        0x17
#define KC_U        0x18
#define KC_V        0x19
#define KC_W        0x1A
#define KC_X        0x1B
#define KC_Y        0x1C
#define KC_Z        0x1D
#define KC_1        0x1E
#define KC_2        0x1F
#define KC_3        0x20
#define KC_4        0x21
#define KC_5        0x22
#define KC_6        0x23
#define KC_7        0x24
#define KC_8        0x25
#define KC_9        0x26
#define KC_0        0x27
#define KC_ENTER    0x28
#define KC_ESC      0x29
#define KC_BSPACE   0x2A
#define KC_TAB      0x2B
#define KC_SPACE    0x2C
#define KC_MINUS    0x2D
#define KC_EQUAL    0x2E
#define KC_F1       0x3A
#define KC_F2       0x3B
#define KC_F3       0x3C
#define KC_F4       0x3D
#define KC_F5       0x3E
#define KC_F6       0x3F
#define KC_F7       0x40
#define KC_F8       0x41
#define KC_F9       0x42
#define KC_F10      0x43
#define KC_F11      0x44
#define KC_F12      0x45
#define KC_PSCREEN  0x46
#define KC_INSERT   0x49
#define KC_HOME     0x4A
#define KC_PGUP     0x4B
#define KC_DELETE   0x4C
#define KC_END      0x4D
#define KC_PGDOWN   0x4E
#define KC_RIGHT    0x4F
#define KC_LEFT     0x50
#define KC_DOWN     0x51
#define KC_UP       0x52
#define KC_APP      0x65    // context menu

// ── Consumer usages (page 0x0C) ──────────────────────────────────────────────
#define CC_NONE         0x0000
#define CC_BRIGHT_UP    0x006F
#define CC_BRIGHT_DOWN  0x0070
#define CC_NEXT_TRACK   0x00B5
#define CC_PREV_TRACK   0x00B6
#define CC_STOP         0x00B7
#define CC_PLAY_PAUSE   0x00CD
#define CC_MUTE         0x00E2
#define CC_VOL_UP       0x00E9
#define CC_VOL_DOWN     0x00EA
#define CC_AL_CALC      0x0192
#define CC_AC_SEARCH    0x0221
#define CC_AC_HOME      0x0223
#define CC_AC_BACK      0x0224
#define CC_AC_FORWARD   0x0225

// ── Mouse buttons ────────────────────────────────────────────────────────────
#define MB_LEFT     0x01
#define MB_RIGHT    0x02
#define MB_MIDDLE   0x04

#endif
