#include "board_io.h"

#include "board.h"
#include "board_config.h"
#include "driver/gpio.h"
#include "driver/ledc.h"
#include "esp_check.h"

static const char *TAG = "board_io";

#define BL_TIMER    LEDC_TIMER_0
#define BL_CHANNEL  LEDC_CHANNEL_0
#define BL_RES      LEDC_TIMER_10_BIT
#define BL_MAX_DUTY ((1u << 10) - 1)

static uint8_t s_backlight_pct;

esp_err_t board_io_init(void)
{
    // LED (GPIO45 strapping pin: only driven after boot, pulled low externally)
    const gpio_config_t led_cfg = {
        .pin_bit_mask = BIT64(USR_LED_PIN),
        .mode = GPIO_MODE_OUTPUT,
    };
    ESP_RETURN_ON_ERROR(gpio_config(&led_cfg), TAG, "led gpio");
    gpio_set_level(USR_LED_PIN, 0);

    // Charge status input
    const gpio_config_t chg_cfg = {
        .pin_bit_mask = BIT64(BAT_CHG_IDC_PIN),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = BAT_CHG_PULLUP ? GPIO_PULLUP_ENABLE : GPIO_PULLUP_DISABLE,
    };
    ESP_RETURN_ON_ERROR(gpio_config(&chg_cfg), TAG, "chg gpio");

    // Backlight PWM (GPIO46 strapping pin, gate pulled low -> off until set)
    const ledc_timer_config_t timer = {
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .duty_resolution = BL_RES,
        .timer_num = BL_TIMER,
        .freq_hz = BACKLIGHT_PWM_HZ,
        .clk_cfg = LEDC_AUTO_CLK,
    };
    ESP_RETURN_ON_ERROR(ledc_timer_config(&timer), TAG, "ledc timer");

    const ledc_channel_config_t channel = {
        .gpio_num = LCD_DIM_PIN,
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .channel = BL_CHANNEL,
        .timer_sel = BL_TIMER,
        .duty = 0,
        .hpoint = 0,
    };
    ESP_RETURN_ON_ERROR(ledc_channel_config(&channel), TAG, "ledc channel");
    s_backlight_pct = 0;

    return ESP_OK;
}

void board_led_set(bool on)
{
    gpio_set_level(USR_LED_PIN, on ? 1 : 0);
}

void board_backlight_set(uint8_t pct)
{
    if (pct > 100) {
        pct = 100;
    }
    s_backlight_pct = pct;
    const uint32_t duty = (BL_MAX_DUTY * pct) / 100;
    ledc_set_duty(LEDC_LOW_SPEED_MODE, BL_CHANNEL, duty);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, BL_CHANNEL);
}

uint8_t board_backlight_get(void)
{
    return s_backlight_pct;
}

bool board_is_charging(void)
{
    const int level = gpio_get_level(BAT_CHG_IDC_PIN);
    return BAT_CHG_ACTIVE_LOW ? (level == 0) : (level != 0);
}
