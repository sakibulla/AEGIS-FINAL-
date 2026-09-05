#include "servo_sweep.h"
#include <driver/ledc.h>
#include <esp_log.h>

static const char *TAG = "SERVO_SWEEP";

#define SERVO_FREQ_HZ      50                 // 50 Hz PWM period (20ms)
#define SERVO_RESOLUTION   LEDC_TIMER_14_BIT  // 16384 total duty cycles
#define SERVO_MIN_PULSE_US 500                // 0 degrees (0.5ms)
#define SERVO_MAX_PULSE_US 2500               // 180 degrees (2.5ms)

esp_err_t init_servo_sweep(void) {
    ledc_timer_config_t timer_conf = {};
    timer_conf.speed_mode      = LEDC_LOW_SPEED_MODE;
    timer_conf.duty_resolution = SERVO_RESOLUTION;
    timer_conf.timer_num       = LEDC_TIMER_1;
    timer_conf.freq_hz         = SERVO_FREQ_HZ;
    timer_conf.clk_cfg         = LEDC_AUTO_CLK;
    ledc_timer_config(&timer_conf);

    ledc_channel_config_t chan_conf = {};
    chan_conf.gpio_num   = PIN_SERVO_SIG;
    chan_conf.speed_mode = LEDC_LOW_SPEED_MODE;
    chan_conf.channel    = LEDC_CHANNEL_2;
    chan_conf.intr_type  = LEDC_INTR_DISABLE;
    chan_conf.timer_sel  = LEDC_TIMER_1;
    chan_conf.duty       = 0;
    chan_conf.hpoint     = 0;
    ledc_channel_config(&chan_conf);

    set_servo_angle(90); // Default center position
    ESP_LOGI(TAG, "Servo Driver online on GPIO %d.", PIN_SERVO_SIG);
    return ESP_OK;
}

void set_servo_angle(uint8_t angle_deg) {
    if (angle_deg > 180) angle_deg = 180;

    uint32_t pulse_us = SERVO_MIN_PULSE_US + ((SERVO_MAX_PULSE_US - SERVO_MIN_PULSE_US) * angle_deg) / 180;
    uint32_t duty = (pulse_us * 16383) / 20000;

    ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_2, duty);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_2);
}