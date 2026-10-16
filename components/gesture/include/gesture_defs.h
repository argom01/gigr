// ─────────────────────────────────────────────────────────────────────────────
//  GESTURE DICTIONARY  -  the one place to add, remove or tune gestures
// ─────────────────────────────────────────────────────────────────────────────
//
//  GESTURE(id, "edge impulse label", kind, min_prob, hold_ms)
//
//    id        C identifier used in profiles.c (G_...)
//    label     must match the label used in Edge Impulse Studio exactly;
//              the firmware matches model outputs to this list by name, so
//              the order here and in the model does not matter
//    kind      K_IDLE   the "nothing happening" class (exactly one)
//              K_DYN    dynamic gesture: fires once, then waits for idle
//              K_STATIC static pose: fires after being held for hold_ms
//    min_prob  probability needed (2 of the last 3 windows for K_DYN)
//    hold_ms   K_STATIC only
//
//  Adding a gesture:
//    1. record and label it in Edge Impulse, retrain, export the library
//    2. add one GESTURE(...) line below (at most 16 in total)
//    3. optionally give it actions in profiles.c (missing = no action)
//
//  Do not train the air-mouse pose (ring + pinky curled): it is detected by
//  rules and gestures are ignored while the air mouse is active.

//       id                  label                kind      min_prob  hold_ms
GESTURE(G_IDLE,              "idle",              K_IDLE,   0.00f,    0)
GESTURE(G_SWIPE_LEFT,        "swipe_left",        K_DYN,    0.80f,    0)
GESTURE(G_SWIPE_RIGHT,       "swipe_right",       K_DYN,    0.80f,    0)
GESTURE(G_SWIPE_UP,          "swipe_up",          K_DYN,    0.80f,    0)
GESTURE(G_SWIPE_DOWN,        "swipe_down",        K_DYN,    0.80f,    0)
GESTURE(G_FIST,              "fist",              K_STATIC, 0.85f,    400)
GESTURE(G_V_SIGN,            "v_sign",            K_STATIC, 0.85f,    400)
GESTURE(G_THUMBS_UP,         "thumbs_up",         K_STATIC, 0.85f,    400)
// Free slots for finger-configuration dependent dynamic gestures, e.g.:
// GESTURE(G_POINT_SWIPE_LEFT,  "point_swipe_left",  K_DYN,    0.80f,    0)
// GESTURE(G_POINT_SWIPE_RIGHT, "point_swipe_right", K_DYN,    0.80f,    0)
// GESTURE(G_PINCH_FLICK,       "pinch_flick",       K_DYN,    0.80f,    0)
