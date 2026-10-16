#ifndef BOARD_H
#define BOARD_H

// ── Pin map ──────────────────────────────────────────────────────────────────

#define USR_LED_PIN 45          // strapping pin, pulled to GND; LED lit on HIGH

#define LCD_TE_PIN 3
#define LCD_DIM_PIN 46          // strapping pin, pulled low by the backlight MOSFET gate resistor
#define LCD_DC_PIN 9
#define LCD_CS_PIN 10           // SPI2 IOMUX
#define LCD_MOSI_PIN 11         // SPI2 IOMUX
#define LCD_SCK_PIN 12          // SPI2 IOMUX
#define LCD_MISO_PIN 13         // SPI2 IOMUX
#define LCD_RST_PIN 38

#define FLEX_1_PIN 4            // pinky
#define FLEX_2_PIN 5            // ring
#define FLEX_3_PIN 1            // index
#define FLEX_4_PIN 2            // middle
#define FLEX_THMB_PIN 6         // thumb (divider inverted: fixed resistor to VDD)

#define BAT_GUAGE_PIN 7
#define BAT_GAUGE_PIN BAT_GUAGE_PIN
#define BAT_CHG_IDC_PIN 39

#define TP_RST_PIN 18
#define TP_INT_PIN 8

#define IMU_INT_PIN 40

#define I2C_SDA_PIN 41
#define I2C_SCL_PIN 42

// ── ADC1 channels (ESP32-S3: GPIOn -> ADC1_CH(n-1) for GPIO1..10) ────────────

#define FLEX_1_ADC_CH 3
#define FLEX_2_ADC_CH 4
#define FLEX_3_ADC_CH 0
#define FLEX_4_ADC_CH 1
#define FLEX_THMB_ADC_CH 5
#define BAT_ADC_CH 6

// ── Fingers ──────────────────────────────────────────────────────────────────
// Logical finger order used everywhere in the firmware (and in the ML input).

typedef enum {
    FINGER_THUMB = 0,
    FINGER_INDEX,
    FINGER_MIDDLE,
    FINGER_RING,
    FINGER_PINKY,
    FINGER_COUNT
} finger_t;

// ADC channel of each logical finger, indexed by finger_t
#define FINGER_ADC_CHANNELS { FLEX_THMB_ADC_CH, FLEX_3_ADC_CH, FLEX_4_ADC_CH, FLEX_2_ADC_CH, FLEX_1_ADC_CH }

// true where the flex sensor is on the low side of the divider (voltage rises with bend)
#define FINGER_VOLTAGE_RISES_ON_BEND { 1, 0, 0, 0, 0 }

// ── Display geometry ─────────────────────────────────────────────────────────

#define LCD_NATIVE_H_RES 320
#define LCD_NATIVE_V_RES 480
#define LCD_H_RES 480           // landscape as mounted on the glove
#define LCD_V_RES 320

#endif
