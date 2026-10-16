#include "flex_adc.h"

#include <string.h>
#include "board_config.h"
#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"
#include "esp_adc/adc_continuous.h"
#include "esp_check.h"
#include "esp_log.h"

static const char *TAG = "flex_adc";

#define N_CH        (FINGER_COUNT + 1)          // fingers + battery
#define BAT_SLOT    FINGER_COUNT
#define FRAME_BYTES (N_CH * 10 * SOC_ADC_DIGI_RESULT_BYTES)
#define READ_BYTES  (FRAME_BYTES * 4)

static adc_continuous_handle_t s_adc;
static adc_cali_handle_t s_cali[N_CH];
static uint8_t s_channel[N_CH];             // ADC channel per slot
static int8_t s_slot_of_channel[10];        // reverse lookup, -1 = unused
static uint8_t s_buf[READ_BYTES];
static float s_last_mv[N_CH];

static esp_err_t check_pin(int gpio, uint8_t expected_ch)
{
    adc_unit_t unit;
    adc_channel_t ch;
    ESP_RETURN_ON_ERROR(adc_continuous_io_to_channel(gpio, &unit, &ch), TAG, "gpio %d is not an ADC pin", gpio);
    if (unit != ADC_UNIT_1 || ch != expected_ch) {
        ESP_LOGE(TAG, "board.h mismatch: GPIO%d is ADC%d_CH%d, expected ADC1_CH%d", gpio, unit + 1, ch, expected_ch);
        return ESP_ERR_INVALID_ARG;
    }
    return ESP_OK;
}

esp_err_t flex_adc_init(void)
{
    ESP_RETURN_ON_ERROR(check_pin(FLEX_THMB_PIN, FLEX_THMB_ADC_CH), TAG, "thumb");
    ESP_RETURN_ON_ERROR(check_pin(FLEX_3_PIN, FLEX_3_ADC_CH), TAG, "index");
    ESP_RETURN_ON_ERROR(check_pin(FLEX_4_PIN, FLEX_4_ADC_CH), TAG, "middle");
    ESP_RETURN_ON_ERROR(check_pin(FLEX_2_PIN, FLEX_2_ADC_CH), TAG, "ring");
    ESP_RETURN_ON_ERROR(check_pin(FLEX_1_PIN, FLEX_1_ADC_CH), TAG, "pinky");
    ESP_RETURN_ON_ERROR(check_pin(BAT_GAUGE_PIN, BAT_ADC_CH), TAG, "battery");

    const uint8_t finger_ch[FINGER_COUNT] = FINGER_ADC_CHANNELS;
    memset(s_slot_of_channel, -1, sizeof(s_slot_of_channel));
    for (int i = 0; i < FINGER_COUNT; i++) {
        s_channel[i] = finger_ch[i];
    }
    s_channel[BAT_SLOT] = BAT_ADC_CH;
    for (int i = 0; i < N_CH; i++) {
        s_slot_of_channel[s_channel[i]] = (int8_t)i;
    }

    const adc_continuous_handle_cfg_t handle_cfg = {
        .max_store_buf_size = READ_BYTES * 2,
        .conv_frame_size = FRAME_BYTES,
        .flags.flush_pool = 1,
    };
    ESP_RETURN_ON_ERROR(adc_continuous_new_handle(&handle_cfg, &s_adc), TAG, "handle");

    adc_digi_pattern_config_t pattern[N_CH] = { 0 };
    for (int i = 0; i < N_CH; i++) {
        pattern[i].atten = ADC_ATTEN_DB_12;
        pattern[i].channel = s_channel[i];
        pattern[i].unit = ADC_UNIT_1;
        pattern[i].bit_width = SOC_ADC_DIGI_MAX_BITWIDTH;
    }
    const adc_continuous_config_t cfg = {
        .pattern_num = N_CH,
        .adc_pattern = pattern,
        .sample_freq_hz = FLEX_ADC_TOTAL_HZ,
        .conv_mode = ADC_CONV_SINGLE_UNIT_1,
        .format = ADC_DIGI_OUTPUT_FORMAT_TYPE2,
    };
    ESP_RETURN_ON_ERROR(adc_continuous_config(s_adc, &cfg), TAG, "config");

    for (int i = 0; i < N_CH; i++) {
        const adc_cali_curve_fitting_config_t cali_cfg = {
            .unit_id = ADC_UNIT_1,
            .chan = s_channel[i],
            .atten = ADC_ATTEN_DB_12,
            .bitwidth = ADC_BITWIDTH_12,
        };
        if (adc_cali_create_scheme_curve_fitting(&cali_cfg, &s_cali[i]) != ESP_OK) {
            ESP_LOGW(TAG, "no eFuse calibration for CH%d, using a linear estimate", s_channel[i]);
            s_cali[i] = NULL;
        }
    }

    ESP_RETURN_ON_ERROR(adc_continuous_start(s_adc), TAG, "start");
    ESP_LOGI(TAG, "%d channels at %d Hz total", N_CH, FLEX_ADC_TOTAL_HZ);
    return ESP_OK;
}

static float raw_to_mv(int slot, uint32_t raw)
{
    int mv;
    if (s_cali[slot] && adc_cali_raw_to_voltage(s_cali[slot], (int)raw, &mv) == ESP_OK) {
        return (float)mv;
    }
    return (float)raw * 3100.0f / 4095.0f;
}

int flex_adc_poll(float finger_mv[FINGER_COUNT], float *bat_adc_mv)
{
    uint32_t sum[N_CH] = { 0 };
    uint32_t cnt[N_CH] = { 0 };

    for (;;) {
        uint32_t len = 0;
        const esp_err_t err = adc_continuous_read(s_adc, s_buf, sizeof(s_buf), &len, 0);
        if (err != ESP_OK || len == 0) {
            break;
        }
        for (uint32_t i = 0; i + SOC_ADC_DIGI_RESULT_BYTES <= len; i += SOC_ADC_DIGI_RESULT_BYTES) {
            const adc_digi_output_data_t *p = (const adc_digi_output_data_t *)&s_buf[i];
            const uint32_t ch = p->type2.channel;
            if (p->type2.unit != 0 || ch >= sizeof(s_slot_of_channel) || s_slot_of_channel[ch] < 0) {
                continue;
            }
            const int slot = s_slot_of_channel[ch];
            sum[slot] += p->type2.data;
            cnt[slot]++;
        }
        if (len < sizeof(s_buf)) {
            break;
        }
    }

    for (int i = 0; i < N_CH; i++) {
        if (cnt[i]) {
            s_last_mv[i] = raw_to_mv(i, (sum[i] + cnt[i] / 2) / cnt[i]);
        }
    }
    memcpy(finger_mv, s_last_mv, sizeof(float) * FINGER_COUNT);
    if (bat_adc_mv) {
        *bat_adc_mv = s_last_mv[BAT_SLOT];
    }
    return (int)cnt[0];
}
