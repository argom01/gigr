#include <stdio.h>
#include <string.h>
#include "ble_hid.h"
#include "board_io.h"
#include "hid_codes.h"
#include "hid_tx.h"
#include "display.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "gui_priv.h"

static lv_obj_t *s_cpu[2];
static lv_obj_t *s_cpu_lbl;
static lv_obj_t *s_heap;
static lv_obj_t *s_tasks;
static uint32_t s_task_refresh;

static const char *const TIMEOUT_OPTS = "Never\n15 s\n30 s\n1 min\n2 min\n5 min";
static const uint16_t TIMEOUT_VALUES[] = { 0, 15, 30, 60, 120, 300 };
#define TIMEOUT_COUNT (sizeof(TIMEOUT_VALUES) / sizeof(TIMEOUT_VALUES[0]))

static int timeout_index(uint16_t s)
{
    for (size_t i = 0; i < TIMEOUT_COUNT; i++) {
        if (TIMEOUT_VALUES[i] == s) {
            return (int)i;
        }
    }
    return 0;
}

static void on_backlight(float v)
{
    const uint8_t pct = (uint8_t)(v + 0.5f);
    board_backlight_set(pct);
    GUI_SETTINGS_EDIT(s->backlight_pct = pct);
}

static void on_dim(int idx)
{
    GUI_SETTINGS_EDIT(s->dim_timeout_s = TIMEOUT_VALUES[idx]);
}

static void on_off(int idx)
{
    GUI_SETTINGS_EDIT(s->off_timeout_s = TIMEOUT_VALUES[idx]);
}

static void on_overlay(bool v)
{
    if (v) {
        lv_sysmon_show_performance(display_get());
    } else {
        lv_sysmon_hide_performance(display_get());
    }
    GUI_SETTINGS_EDIT(s->perf_overlay = v);
}

static void on_led(bool v)
{
    GUI_SETTINGS_EDIT(s->led_enabled = v);
}

static void on_reboot(lv_event_t *e)
{
    (void)e;
    settings_save_now();
    esp_restart();
}

static lv_obj_t *s_ble;

static void on_hid_key(lv_event_t *e)
{
    (void)e;
    hid_tx_key(MOD_NONE, KC_A);
}

static void on_hid_vol(lv_event_t *e)
{
    (void)e;
    hid_tx_consumer(CC_VOL_UP);
}

static void mouse_square_step(lv_timer_t *t)
{
    // small square so the motion is easy to see on the host
    static const int8_t moves[4][2] = { { 40, 0 }, { 0, 40 }, { -40, 0 }, { 0, -40 } };
    int *step = lv_timer_get_user_data(t);
    hid_tx_mouse_move(moves[*step][0], moves[*step][1], 0, 0);
    *step = (*step + 1) % 4;
}

static void on_hid_mouse(lv_event_t *e)
{
    (void)e;
    static int step;
    step = 0;
    lv_timer_t *t = lv_timer_create(mouse_square_step, 150, &step);
    lv_timer_set_repeat_count(t, 4);
    lv_timer_set_auto_delete(t, true);
}

static void on_hid_scroll(lv_event_t *e)
{
    (void)e;
    hid_tx_mouse_move(0, 0, -3, 0);
}

static void on_clear_bonds(lv_event_t *e)
{
    (void)e;
    hid_device_clear_bonds();
}

static void on_touch_test(lv_event_t *e)
{
    (void)e;
    gui_touchtest_open();
}

static void on_reset_confirm(lv_event_t *e)
{
    lv_obj_t *mbox = lv_event_get_user_data(e);
    settings_t def;
    settings_defaults(&def);
    settings_set(&def);
    settings_save_now();
    lv_msgbox_close(mbox);
    esp_restart();
}

static void on_reset(lv_event_t *e)
{
    (void)e;
    lv_obj_t *mbox = lv_msgbox_create(NULL);
    lv_msgbox_add_title(mbox, "Reset settings");
    lv_msgbox_add_text(mbox, "Erase calibration and all settings, then reboot?");
    lv_obj_t *yes = lv_msgbox_add_footer_button(mbox, "Reset");
    lv_obj_add_event_cb(yes, on_reset_confirm, LV_EVENT_CLICKED, mbox);
    lv_msgbox_add_close_button(mbox);
}

void scr_system_create(lv_obj_t *tab)
{
    settings_t cfg;
    settings_get(&cfg);

    lv_obj_t *disp = gui_section(tab, "Display");
    gui_slider(disp, "Backlight %", 5, 100, cfg.backlight_pct, 0, on_backlight);
    gui_dropdown(disp, "Dim after", TIMEOUT_OPTS, timeout_index(cfg.dim_timeout_s), on_dim);
    gui_dropdown(disp, "Off after", TIMEOUT_OPTS, timeout_index(cfg.off_timeout_s), on_off);
    gui_switch(disp, "FPS / CPU overlay", cfg.perf_overlay, on_overlay);
    gui_switch(disp, "Status LED", cfg.led_enabled, on_led);

    lv_obj_t *ble = gui_section(tab, "Bluetooth HID test");
    s_ble = gui_label(ble, "");
    lv_obj_t *hrow = gui_row(ble);
    gui_button(hrow, "Type 'a'", on_hid_key, NULL);
    gui_button(hrow, "Vol +", on_hid_vol, NULL);
    gui_button(hrow, "Move mouse", on_hid_mouse, NULL);
    gui_button(hrow, "Scroll", on_hid_scroll, NULL);
    gui_button(hrow, LV_SYMBOL_TRASH " Forget hosts", on_clear_bonds, NULL);

    lv_obj_t *perf = gui_section(tab, "Performance");
    s_cpu_lbl = gui_label(perf, "");
    s_cpu[0] = gui_bar(perf, 0, 100);
    s_cpu[1] = gui_bar(perf, 0, 100);
    s_heap = gui_label(perf, "");

    lv_obj_t *tasks = gui_section(tab, "Tasks (name core prio stack-free)");
    s_tasks = gui_label(tasks, "");
    lv_obj_set_style_text_font(s_tasks, &lv_font_montserrat_12, 0);

    lv_obj_t *act = gui_section(tab, "Device");
    lv_obj_t *row = gui_row(act);
    gui_button(row, LV_SYMBOL_EDIT " Touch test", on_touch_test, NULL);
    gui_button(row, LV_SYMBOL_REFRESH " Reboot", on_reboot, NULL);
    gui_button(row, LV_SYMBOL_TRASH " Reset settings", on_reset, NULL);
}

static void update_tasks(void)
{
    enum { MAX_TASKS = 24 };
    static TaskStatus_t status[MAX_TASKS];
    static char text[1024];

    const UBaseType_t n = uxTaskGetSystemState(status, MAX_TASKS, NULL);
    size_t len = 0;
    for (UBaseType_t i = 0; i < n && len < sizeof(text) - 48; i++) {
        const BaseType_t core = status[i].xCoreID;
        len += snprintf(text + len, sizeof(text) - len, "%-10s %c %2u %5u\n", status[i].pcTaskName,
                        core == tskNO_AFFINITY ? '*' : (char)('0' + core), (unsigned)status[i].uxCurrentPriority,
                        (unsigned)status[i].usStackHighWaterMark);
    }
    lv_label_set_text(s_tasks, text);
}

void scr_system_update(const app_state_t *st)
{
    lv_label_set_text_fmt(s_ble, "%s  reports sent %lu  dropped %lu",
                          st->ble.connected ? "connected" : (st->ble.advertising ? "advertising" : "idle"),
                          (unsigned long)st->ble.reports_sent, (unsigned long)st->ble.reports_dropped);
    lv_label_set_text_fmt(s_cpu_lbl, "CPU core0 %.0f%%  core1 %.0f%%   uptime %lu s", (double)st->sys.cpu_load[0],
                          (double)st->sys.cpu_load[1], (unsigned long)st->sys.uptime_s);
    lv_bar_set_value(s_cpu[0], (int32_t)st->sys.cpu_load[0], LV_ANIM_OFF);
    lv_bar_set_value(s_cpu[1], (int32_t)st->sys.cpu_load[1], LV_ANIM_OFF);
    lv_label_set_text_fmt(s_heap, "Internal free %lu kB (min %lu kB)  PSRAM free %lu kB",
                          (unsigned long)(st->sys.heap_int_free / 1024), (unsigned long)(st->sys.heap_int_min / 1024),
                          (unsigned long)(st->sys.heap_psram_free / 1024));

    if (++s_task_refresh >= 40) {   // every 2 s
        s_task_refresh = 0;
        update_tasks();
    } else if (s_task_refresh == 1 && lv_label_get_text(s_tasks)[0] == '\0') {
        update_tasks();
    }
}
