#include "gui_priv.h"

// Full-screen crosshair for checking the touch orientation (TOUCH_* in board_config.h)

static lv_obj_t *s_scr;
static lv_obj_t *s_hline;
static lv_obj_t *s_vline;
static lv_obj_t *s_info;
static lv_timer_t *s_timer;

static void close_cb(lv_event_t *e)
{
    (void)e;
    lv_timer_delete(s_timer);
    s_timer = NULL;
    lv_screen_load(gui_main_screen());
    lv_obj_delete_async(s_scr);
    s_scr = NULL;
}

static void timer_cb(lv_timer_t *t)
{
    (void)t;
    app_touch_state_t ts;
    APP_STATE_TAKE(touch, &ts);
    const bool down = ts.points > 0;
    lv_obj_set_hidden(s_hline, !down);
    lv_obj_set_hidden(s_vline, !down);
    if (down) {
        lv_obj_set_pos(s_hline, 0, ts.y);
        lv_obj_set_pos(s_vline, ts.x, 0);
    }
    lv_label_set_text_fmt(s_info, "x %u  y %u  points %u  irq %lu\n"
                                  "Touch where you look; wrong axis?\n"
                                  "edit TOUCH_SWAP_XY / MIRROR_X / MIRROR_Y",
                          ts.x, ts.y, ts.points, (unsigned long)ts.irq_count);
}

static lv_obj_t *line_obj(lv_obj_t *parent, int32_t w, int32_t h)
{
    lv_obj_t *o = lv_obj_create(parent);
    lv_obj_remove_style_all(o);
    lv_obj_set_size(o, w, h);
    lv_obj_set_style_bg_opa(o, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(o, GUI_COL_ACCENT, 0);
    lv_obj_set_clickable(o, false);
    lv_obj_set_hidden(o, true);
    return o;
}

void gui_touchtest_open(void)
{
    if (s_scr) {
        return;
    }
    s_scr = lv_obj_create(NULL);
    lv_obj_set_scrollable(s_scr, false);

    // corner markers: the top-left one must be under your finger at (0,0)
    lv_obj_t *tl = gui_label(s_scr, "(0,0)");
    lv_obj_align(tl, LV_ALIGN_TOP_LEFT, 2, 2);
    lv_obj_t *br = gui_label(s_scr, "(479,319)");
    lv_obj_align(br, LV_ALIGN_BOTTOM_RIGHT, -2, -2);

    s_info = gui_label(s_scr, "");
    lv_obj_center(s_info);
    s_hline = line_obj(s_scr, LV_PCT(100), 2);
    s_vline = line_obj(s_scr, 2, LV_PCT(100));

    lv_obj_t *btn = gui_button(s_scr, LV_SYMBOL_CLOSE " Exit", close_cb, NULL);
    lv_obj_align(btn, LV_ALIGN_TOP_RIGHT, -4, 24);

    s_timer = lv_timer_create(timer_cb, 30, NULL);
    lv_screen_load(s_scr);
}
