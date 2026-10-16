#ifndef APP_REQ_H
#define APP_REQ_H

#include <stdbool.h>

// One-shot requests from the GUI to the background tasks. Posting sets a bit,
// the owning task takes (reads and clears) it on its next iteration.

typedef enum {
    REQ_FLEX_CAL_OPEN = 0,      // capture "hand flat, fingers straight"
    REQ_FLEX_CAL_FIST,          // capture "fist" (index..pinky closed values)
    REQ_FLEX_CAL_THUMB,         // capture "thumb tucked across palm" (thumb closed value)
    REQ_FLEX_CAL_SAVE,          // validate captured values and store them
    REQ_FLEX_REZERO,            // quick re-zero: capture open pose, keep the span
    REQ_GYRO_CAL,               // force a gyro bias capture (hold still)
    REQ_LEVEL,                  // capture the mean gravity -> levelling rotation
    REQ_LEVEL_RESET,            // forget the levelling rotation
    REQ_FUSION_RESET,           // reset the orientation filter
    REQ_TEST_CURSOR_RESET,      // re-centre the air-mouse test pad cursor
    REQ_COUNT
} app_req_t;

void app_req_post(app_req_t req);
bool app_req_take(app_req_t req);
bool app_req_pending(app_req_t req);

#endif
