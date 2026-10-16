#include <stdio.h>
#include <stdlib.h>
#include "gui_priv.h"

lv_obj_t *gui_column(lv_obj_t *parent)
{
    lv_obj_t *c = lv_obj_create(parent);
    lv_obj_remove_style_all(c);
    lv_obj_set_size(c, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(c, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(c, 6, 0);
    return c;
}

lv_obj_t *gui_row(lv_obj_t *parent)
{
    lv_obj_t *r = lv_obj_create(parent);
    lv_obj_remove_style_all(r);
    lv_obj_set_size(r, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(r, LV_FLEX_FLOW_ROW_WRAP);
    lv_obj_set_flex_align(r, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(r, 8, 0);
    lv_obj_set_style_pad_row(r, 6, 0);
    return r;
}

lv_obj_t *gui_section(lv_obj_t *parent, const char *title)
{
    lv_obj_t *s = lv_obj_create(parent);
    lv_obj_set_size(s, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(s, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(s, 8, 0);
    lv_obj_set_style_pad_row(s, 6, 0);
    lv_obj_set_scrollable(s, false);
    if (title) {
        lv_obj_t *t = lv_label_create(s);
        lv_label_set_text(t, title);
        lv_obj_set_style_text_color(t, GUI_COL_ACCENT, 0);
    }
    return s;
}

lv_obj_t *gui_label(lv_obj_t *parent, const char *text)
{
    lv_obj_t *l = lv_label_create(parent);
    lv_label_set_text(l, text ? text : "");
    return l;
}

lv_obj_t *gui_title(lv_obj_t *parent, const char *text)
{
    lv_obj_t *l = gui_label(parent, text);
    lv_obj_set_style_text_font(l, &lv_font_montserrat_20, 0);
    return l;
}

lv_obj_t *gui_button(lv_obj_t *parent, const char *text, lv_event_cb_t cb, void *user_data)
{
    lv_obj_t *b = lv_button_create(parent);
    lv_obj_set_style_min_height(b, 40, 0);
    lv_obj_t *l = lv_label_create(b);
    lv_label_set_text(l, text);
    lv_obj_center(l);
    if (cb) {
        lv_obj_add_event_cb(b, cb, LV_EVENT_CLICKED, user_data);
    }
    return b;
}

static void switch_event(lv_event_t *e)
{
    gui_bool_cb_t cb = (gui_bool_cb_t)lv_event_get_user_data(e);
    lv_obj_t *sw = lv_event_get_target_obj(e);
    if (cb) {
        cb(lv_obj_has_state(sw, LV_STATE_CHECKED));
    }
}

lv_obj_t *gui_switch(lv_obj_t *parent, const char *text, bool state, gui_bool_cb_t cb)
{
    lv_obj_t *row = gui_row(parent);
    lv_obj_t *sw = lv_switch_create(row);
    if (state) {
        lv_obj_add_state(sw, LV_STATE_CHECKED);
    }
    lv_obj_add_event_cb(sw, switch_event, LV_EVENT_VALUE_CHANGED, (void *)cb);
    gui_label(row, text);
    return sw;
}

typedef struct {
    float min;
    float max;
    int decimals;
    const char *text;
    lv_obj_t *label;
    gui_float_cb_t cb;
} slider_ctx_t;

#define SLIDER_STEPS 1000

static float slider_value(lv_obj_t *slider, const slider_ctx_t *ctx)
{
    const int32_t raw = lv_slider_get_value(slider);
    return ctx->min + (ctx->max - ctx->min) * (float)raw / SLIDER_STEPS;
}

static void slider_label_update(lv_obj_t *slider, const slider_ctx_t *ctx)
{
    lv_label_set_text_fmt(ctx->label, "%s: %.*f", ctx->text, ctx->decimals, (double)slider_value(slider, ctx));
}

static void slider_event(lv_event_t *e)
{
    lv_obj_t *slider = lv_event_get_target_obj(e);
    slider_ctx_t *ctx = lv_event_get_user_data(e);
    slider_label_update(slider, ctx);
    if (lv_event_get_code(e) == LV_EVENT_RELEASED && ctx->cb) {
        ctx->cb(slider_value(slider, ctx));
    }
}

lv_obj_t *gui_slider(lv_obj_t *parent, const char *text, float min, float max, float value, int decimals,
                     gui_float_cb_t cb)
{
    lv_obj_t *col = gui_column(parent);
    slider_ctx_t *ctx = calloc(1, sizeof(*ctx));
    ctx->min = min;
    ctx->max = max;
    ctx->decimals = decimals;
    ctx->text = text;
    ctx->cb = cb;
    ctx->label = gui_label(col, "");

    lv_obj_t *s = lv_slider_create(col);
    lv_obj_set_width(s, LV_PCT(92));
    lv_obj_set_style_margin_left(s, 10, 0);
    lv_slider_set_range(s, 0, SLIDER_STEPS);
    lv_obj_set_user_data(s, ctx);
    gui_slider_set(s, value);
    lv_obj_add_event_cb(s, slider_event, LV_EVENT_VALUE_CHANGED, ctx);
    lv_obj_add_event_cb(s, slider_event, LV_EVENT_RELEASED, ctx);
    return s;
}

void gui_slider_set(lv_obj_t *slider, float value)
{
    slider_ctx_t *ctx = lv_obj_get_user_data(slider);
    float t = (value - ctx->min) / (ctx->max - ctx->min);
    if (t < 0.0f) {
        t = 0.0f;
    } else if (t > 1.0f) {
        t = 1.0f;
    }
    lv_slider_set_value(slider, (int32_t)(t * SLIDER_STEPS + 0.5f), LV_ANIM_OFF);
    slider_label_update(slider, ctx);
}

static void dropdown_event(lv_event_t *e)
{
    gui_int_cb_t cb = (gui_int_cb_t)lv_event_get_user_data(e);
    if (cb) {
        cb((int)lv_dropdown_get_selected(lv_event_get_target_obj(e)));
    }
}

lv_obj_t *gui_dropdown(lv_obj_t *parent, const char *text, const char *options, int selected, gui_int_cb_t cb)
{
    lv_obj_t *row = gui_row(parent);
    gui_label(row, text);
    lv_obj_t *dd = lv_dropdown_create(row);
    lv_dropdown_set_options(dd, options);
    lv_dropdown_set_selected(dd, (uint32_t)selected);
    lv_obj_set_width(dd, 170);
    lv_obj_add_event_cb(dd, dropdown_event, LV_EVENT_VALUE_CHANGED, (void *)cb);
    return dd;
}

lv_obj_t *gui_bar(lv_obj_t *parent, int32_t min, int32_t max)
{
    lv_obj_t *b = lv_bar_create(parent);
    lv_bar_set_range(b, min, max);
    lv_obj_set_size(b, LV_PCT(100), 14);
    return b;
}

void gui_set_text_color(lv_obj_t *obj, lv_color_t color)
{
    lv_obj_set_style_text_color(obj, color, 0);
}

const char *gui_hw_text(hw_status_t st)
{
    switch (st) {
    case HW_OK:
        return "OK";
    case HW_MISSING:
        return "MISSING";
    default:
        return "--";
    }
}

lv_color_t gui_hw_color(hw_status_t st)
{
    switch (st) {
    case HW_OK:
        return GUI_COL_OK;
    case HW_MISSING:
        return GUI_COL_BAD;
    default:
        return GUI_COL_DIM;
    }
}
