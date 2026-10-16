#ifndef GUI_H
#define GUI_H

// Builds the LVGL user interface. Call after display_init() and before
// display_start(); afterwards everything runs inside the GUI task.
void gui_init(void);

#endif
