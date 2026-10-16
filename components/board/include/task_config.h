#ifndef TASK_CONFIG_H
#define TASK_CONFIG_H

// Task placement. Core 0: Bluetooth, HID transmit, touch, GUI, housekeeping.
// Core 1: deterministic sensing (200 Hz) and gesture inference, so neither
// shares a core with Bluedroid bursts or LVGL software rendering.

#define TASK_CORE_HID_TX        0
#define TASK_PRIO_HID_TX        15
#define TASK_STACK_HID_TX       4096

#define TASK_CORE_TOUCH         0
#define TASK_PRIO_TOUCH         12
#define TASK_STACK_TOUCH        4096

#define TASK_CORE_GUI           0
#define TASK_PRIO_GUI           4
#define TASK_STACK_GUI          8192

#define TASK_CORE_SYSMON        0
#define TASK_PRIO_SYSMON        2
#define TASK_STACK_SYSMON       4096

#define TASK_CORE_SENSOR        1
#define TASK_PRIO_SENSOR        20
#define TASK_STACK_SENSOR       6144

#define TASK_CORE_INFER         1
#define TASK_PRIO_INFER         10
#define TASK_STACK_INFER        12288   // Edge Impulse run_classifier needs room

#define TASK_CORE_CAPTURE       1
#define TASK_PRIO_CAPTURE       8
#define TASK_STACK_CAPTURE      4096

#endif
