#include "imu.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include "board.h"
#include "board_config.h"
#include "driver/gpio.h"
#include "driver_mpu6500.h"
#include "esp_attr.h"
#include "esp_check.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "i2c_bus.h"

static const char *TAG = "imu";

#define IMU_TIMEOUT_MS 10
#define REG_ACCEL_XOUT_H 0x3B

static i2c_master_dev_handle_t s_dev;
static mpu6500_handle_t s_mpu;

#if IMU_ACCEL_RANGE_G == 4
#define ACCEL_LSB_PER_G 8192.0f
#define ACCEL_RANGE MPU6500_ACCELEROMETER_RANGE_4G
#else
#error "unsupported IMU_ACCEL_RANGE_G"
#endif

#if IMU_GYRO_RANGE_DPS == 1000
#define GYRO_LSB_PER_DPS 32.8f
#define GYRO_RANGE MPU6500_GYROSCOPE_RANGE_1000DPS
#else
#error "unsupported IMU_GYRO_RANGE_DPS"
#endif

// ── libdriver interface shim (addresses are 8-bit write addresses) ───────────

static uint8_t shim_iic_init(void)
{
    return s_dev ? 0 : 1;
}

static uint8_t shim_iic_deinit(void)
{
    return 0;
}

static uint8_t shim_iic_read(uint8_t addr, uint8_t reg, uint8_t *buf, uint16_t len)
{
    (void)addr;
    return i2c_master_transmit_receive(s_dev, &reg, 1, buf, len, IMU_TIMEOUT_MS) == ESP_OK ? 0 : 1;
}

static uint8_t shim_iic_write(uint8_t addr, uint8_t reg, uint8_t *buf, uint16_t len)
{
    (void)addr;
    uint8_t tmp[32];
    if (len + 1 > sizeof(tmp)) {
        return 1;
    }
    tmp[0] = reg;
    memcpy(&tmp[1], buf, len);
    return i2c_master_transmit(s_dev, tmp, len + 1, IMU_TIMEOUT_MS) == ESP_OK ? 0 : 1;
}

static uint8_t shim_spi_init(void)
{
    return 1;
}

static uint8_t shim_spi_read(uint8_t reg, uint8_t *buf, uint16_t len)
{
    (void)reg;
    (void)buf;
    (void)len;
    return 1;
}

static uint8_t shim_spi_write(uint8_t reg, uint8_t *buf, uint16_t len)
{
    (void)reg;
    (void)buf;
    (void)len;
    return 1;
}

static void shim_delay_ms(uint32_t ms)
{
    vTaskDelay(pdMS_TO_TICKS(ms) ? pdMS_TO_TICKS(ms) : 1);
}

static void shim_debug_print(const char *const fmt, ...)
{
    char buf[128];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    ESP_LOGD(TAG, "%s", buf);
}

static void shim_receive_callback(uint8_t type)
{
    (void)type;
}

// ── Public API ───────────────────────────────────────────────────────────────

#define MPU_TRY(call, what)                                  \
    do {                                                     \
        if ((call) != 0) {                                   \
            ESP_LOGE(TAG, "%s failed", what);                \
            return ESP_FAIL;                                 \
        }                                                    \
    } while (0)

esp_err_t imu_init(void)
{
    i2c_master_bus_handle_t bus = i2c_bus_get();
    ESP_RETURN_ON_FALSE(bus, ESP_ERR_INVALID_STATE, TAG, "i2c bus not initialised");

    if (i2c_master_probe(bus, IMU_I2C_ADDR, 20) != ESP_OK) {
        ESP_LOGE(TAG, "no device at 0x%02x", IMU_I2C_ADDR);
        return ESP_ERR_NOT_FOUND;
    }

    const i2c_device_config_t dev_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = IMU_I2C_ADDR,
        .scl_speed_hz = I2C_BUS_HZ,
    };
    ESP_RETURN_ON_ERROR(i2c_master_bus_add_device(bus, &dev_cfg, &s_dev), TAG, "add device");

    DRIVER_MPU6500_LINK_INIT(&s_mpu, mpu6500_handle_t);
    DRIVER_MPU6500_LINK_IIC_INIT(&s_mpu, shim_iic_init);
    DRIVER_MPU6500_LINK_IIC_DEINIT(&s_mpu, shim_iic_deinit);
    DRIVER_MPU6500_LINK_IIC_READ(&s_mpu, shim_iic_read);
    DRIVER_MPU6500_LINK_IIC_WRITE(&s_mpu, shim_iic_write);
    DRIVER_MPU6500_LINK_SPI_INIT(&s_mpu, shim_spi_init);
    DRIVER_MPU6500_LINK_SPI_DEINIT(&s_mpu, shim_iic_deinit);
    DRIVER_MPU6500_LINK_SPI_READ(&s_mpu, shim_spi_read);
    DRIVER_MPU6500_LINK_SPI_WRITE(&s_mpu, shim_spi_write);
    DRIVER_MPU6500_LINK_DELAY_MS(&s_mpu, shim_delay_ms);
    DRIVER_MPU6500_LINK_DEBUG_PRINT(&s_mpu, shim_debug_print);
    DRIVER_MPU6500_LINK_RECEIVE_CALLBACK(&s_mpu, shim_receive_callback);

    MPU_TRY(mpu6500_set_interface(&s_mpu, MPU6500_INTERFACE_IIC), "interface");
    MPU_TRY(mpu6500_set_addr_pin(&s_mpu, IMU_I2C_ADDR == 0x69 ? MPU6500_ADDRESS_AD0_HIGH : MPU6500_ADDRESS_AD0_LOW),
            "address");
    if (mpu6500_init(&s_mpu) != 0) {
        ESP_LOGE(TAG, "init failed (WHO_AM_I mismatch or no answer)");
        return ESP_ERR_NOT_FOUND;
    }

    MPU_TRY(mpu6500_set_sleep(&s_mpu, MPU6500_BOOL_FALSE), "wake");
    MPU_TRY(mpu6500_set_clock_source(&s_mpu, MPU6500_CLOCK_SOURCE_PLL), "clock");
    MPU_TRY(mpu6500_set_ptat(&s_mpu, MPU6500_BOOL_TRUE), "temp sensor");
    MPU_TRY(mpu6500_set_cycle_wake_up(&s_mpu, MPU6500_BOOL_FALSE), "cycle");
    const mpu6500_source_t axes[] = { MPU6500_SOURCE_ACC_X,  MPU6500_SOURCE_ACC_Y,  MPU6500_SOURCE_ACC_Z,
                                      MPU6500_SOURCE_GYRO_X, MPU6500_SOURCE_GYRO_Y, MPU6500_SOURCE_GYRO_Z };
    for (size_t i = 0; i < sizeof(axes) / sizeof(axes[0]); i++) {
        MPU_TRY(mpu6500_set_standby_mode(&s_mpu, axes[i], MPU6500_BOOL_FALSE), "standby");
    }
    MPU_TRY(mpu6500_set_gyro_standby(&s_mpu, MPU6500_BOOL_FALSE), "gyro standby");
    MPU_TRY(mpu6500_set_fifo(&s_mpu, MPU6500_BOOL_FALSE), "fifo");
    MPU_TRY(mpu6500_set_accelerometer_range(&s_mpu, ACCEL_RANGE), "accel range");
    MPU_TRY(mpu6500_set_gyroscope_range(&s_mpu, GYRO_RANGE), "gyro range");
    // DLPF on: internal rate 1 kHz, divider 4 -> 200 Hz
    MPU_TRY(mpu6500_set_low_pass_filter(&s_mpu, MPU6500_LOW_PASS_FILTER_3), "gyro dlpf");
    MPU_TRY(mpu6500_set_sample_rate_divider(&s_mpu, (uint8_t)(1000 / IMU_SAMPLE_HZ - 1)), "rate");
    MPU_TRY(mpu6500_set_accelerometer_choice(&s_mpu, 0), "accel fchoice");
    MPU_TRY(mpu6500_set_accelerometer_low_pass_filter(&s_mpu, MPU6500_ACCELEROMETER_LOW_PASS_FILTER_3), "accel dlpf");
    MPU_TRY(mpu6500_set_iic_master(&s_mpu, MPU6500_BOOL_FALSE), "i2c master");

    vTaskDelay(pdMS_TO_TICKS(50));
    ESP_LOGI(TAG, "MPU6500 ready at 0x%02x", IMU_I2C_ADDR);
    return ESP_OK;
}

esp_err_t imu_read(float accel_g[3], float gyro_dps[3], float *temp_c)
{
    uint8_t reg = REG_ACCEL_XOUT_H;
    uint8_t b[14];
    const esp_err_t err = i2c_master_transmit_receive(s_dev, &reg, 1, b, sizeof(b), IMU_TIMEOUT_MS);
    if (err != ESP_OK) {
        return err;
    }
    for (int i = 0; i < 3; i++) {
        const int16_t a = (int16_t)((b[2 * i] << 8) | b[2 * i + 1]);
        const int16_t g = (int16_t)((b[8 + 2 * i] << 8) | b[8 + 2 * i + 1]);
        accel_g[i] = (float)a / ACCEL_LSB_PER_G;
        gyro_dps[i] = (float)g / GYRO_LSB_PER_DPS;
    }
    if (temp_c) {
        const int16_t t = (int16_t)((b[6] << 8) | b[7]);
        *temp_c = (float)t / 333.87f + 21.0f;
    }
    return ESP_OK;
}

// ── Data-ready interrupt ─────────────────────────────────────────────────────

static TaskHandle_t s_drdy_task;
static portMUX_TYPE s_drdy_lock = portMUX_INITIALIZER_UNLOCKED;
static int64_t s_drdy_us;       // 64 bit: not atomic on Xtensa, hence the lock

static void IRAM_ATTR drdy_isr(void *arg)
{
    (void)arg;
    const int64_t now = esp_timer_get_time();
    portENTER_CRITICAL_ISR(&s_drdy_lock);
    s_drdy_us = now;
    portEXIT_CRITICAL_ISR(&s_drdy_lock);
    BaseType_t woken = pdFALSE;
    vTaskNotifyGiveFromISR(s_drdy_task, &woken);
    portYIELD_FROM_ISR(woken);
}

esp_err_t imu_drdy_start(TaskHandle_t task)
{
    ESP_RETURN_ON_FALSE(s_dev && task, ESP_ERR_INVALID_STATE, TAG, "not initialised");

    // INT: active high, push-pull, 50 us pulse per sample. libdriver writes
    // the inverse of `enable` into LATCH_INT_EN, so TRUE selects the pulse.
    MPU_TRY(mpu6500_set_interrupt_level(&s_mpu, MPU6500_PIN_LEVEL_HIGH), "int level");
    MPU_TRY(mpu6500_set_interrupt_pin_type(&s_mpu, MPU6500_PIN_TYPE_PUSH_PULL), "int pin type");
    MPU_TRY(mpu6500_set_interrupt_latch(&s_mpu, MPU6500_BOOL_TRUE), "int pulse");

    s_drdy_task = task;
    const gpio_config_t io = {
        .pin_bit_mask = 1ULL << IMU_INT_PIN,
        .mode = GPIO_MODE_INPUT,
        .pull_down_en = GPIO_PULLDOWN_ENABLE,   // an unconnected pin must not produce edges
        .intr_type = GPIO_INTR_POSEDGE,
    };
    ESP_RETURN_ON_ERROR(gpio_config(&io), TAG, "int gpio");
    // the touch driver may have installed the shared GPIO ISR service already
    const esp_err_t err = gpio_install_isr_service(0);
    ESP_RETURN_ON_FALSE(err == ESP_OK || err == ESP_ERR_INVALID_STATE, err, TAG, "gpio isr service");
    ESP_RETURN_ON_ERROR(gpio_isr_handler_add(IMU_INT_PIN, drdy_isr, NULL), TAG, "int handler");

    MPU_TRY(mpu6500_set_interrupt(&s_mpu, MPU6500_INTERRUPT_DATA_READY, MPU6500_BOOL_TRUE), "int enable");
    ESP_LOGI(TAG, "data-ready interrupt on GPIO%d", IMU_INT_PIN);
    return ESP_OK;
}

int64_t imu_drdy_time_us(void)
{
    portENTER_CRITICAL(&s_drdy_lock);
    const int64_t t = s_drdy_us;
    portEXIT_CRITICAL(&s_drdy_lock);
    return t;
}
