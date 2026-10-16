#include "capture.h"
#include "gui_priv.h"

static lv_obj_t *s_btn_lbl;
static lv_obj_t *s_status;

static void on_toggle(lv_event_t *e)
{
    (void)e;
    if (capture_running()) {
        capture_stop();
    } else {
        capture_start();
    }
}

void scr_capture_create(lv_obj_t *tab)
{
    lv_obj_t *sec = gui_section(tab, "Edge Impulse data capture");
    gui_label(sec, "Streams 11 values per line at 100 Hz on the USB port:\n"
                   "ax ay az (/2 g), gx gy gz (/500 dps),\n"
                   "thumb index middle ring pinky (0..1).\n"
                   "On the PC run:  edge-impulse-data-forwarder\n"
                   "Calibrate the flex sensors first. Logs are muted\n"
                   "while streaming; gestures and air mouse are paused.");
    lv_obj_t *b = gui_button(sec, "", on_toggle, NULL);
    lv_obj_set_width(b, LV_PCT(100));
    s_btn_lbl = lv_obj_get_child(b, 0);
    s_status = gui_label(sec, "");
}

void scr_capture_update(const app_state_t *st)
{
    lv_label_set_text(s_btn_lbl, st->capture.running ? LV_SYMBOL_STOP " Stop streaming" : LV_SYMBOL_PLAY " Start streaming");
    lv_label_set_text_fmt(s_status, "%s   %lu samples (%.1f s)", st->capture.running ? "STREAMING" : "idle",
                          (unsigned long)st->capture.samples, (double)(st->capture.samples / 100.0f));
    gui_set_text_color(s_status, st->capture.running ? GUI_COL_OK : GUI_COL_DIM);
}
