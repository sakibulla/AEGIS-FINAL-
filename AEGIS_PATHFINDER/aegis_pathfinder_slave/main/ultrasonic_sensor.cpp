#include "ultrasonic_sensor.h"
#include <driver/gpio.h>
#include <esp_timer.h>
#include <esp_rom_sys.h>
#include <esp_log.h>
#include <algorithm>

static const char *TAG = "HC_SR04";

#define MAX_TIMEOUT_US 20000 // ~3.4m distance timeout limit

esp_err_t init_ultrasonic_sensor(void) {
    gpio_config_t trig_conf = {};
    trig_conf.pin_bit_mask = (1ULL << PIN_HCSR04_TRIG);
    trig_conf.mode = GPIO_MODE_OUTPUT;
    trig_conf.pull_up_en = GPIO_PULLUP_DISABLE;
    trig_conf.pull_down_en = GPIO_PULLDOWN_ENABLE;
    trig_conf.intr_type = GPIO_INTR_DISABLE;
    gpio_config(&trig_conf);

    gpio_config_t echo_conf = {};
    echo_conf.pin_bit_mask = (1ULL << PIN_HCSR04_ECHO);
    echo_conf.mode = GPIO_MODE_INPUT;
    echo_conf.pull_up_en = GPIO_PULLUP_DISABLE;
    echo_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
    echo_conf.intr_type = GPIO_INTR_DISABLE;
    gpio_config(&echo_conf);

    gpio_set_level((gpio_num_t)PIN_HCSR04_TRIG, 0);
    ESP_LOGI(TAG, "HC-SR04 Ultrasonic Driver online on TRIG:%d ECHO:%d.", PIN_HCSR04_TRIG, PIN_HCSR04_ECHO);
    return ESP_OK;
}

static uint16_t single_ping_mm(void) {
    gpio_set_level((gpio_num_t)PIN_HCSR04_TRIG, 0);
    esp_rom_delay_us(2);
    gpio_set_level((gpio_num_t)PIN_HCSR04_TRIG, 1);
    esp_rom_delay_us(10);
    gpio_set_level((gpio_num_t)PIN_HCSR04_TRIG, 0);

    int64_t start_time = esp_timer_get_time();
    while (gpio_get_level((gpio_num_t)PIN_HCSR04_ECHO) == 0) {
        if ((esp_timer_get_time() - start_time) > MAX_TIMEOUT_US) return 0;
    }

    int64_t echo_start = esp_timer_get_time();
    while (gpio_get_level((gpio_num_t)PIN_HCSR04_ECHO) == 1) {
        if ((esp_timer_get_time() - echo_start) > MAX_TIMEOUT_US) return 0;
    }

    int64_t echo_duration = esp_timer_get_time() - echo_start;
    uint16_t dist_mm = (uint16_t)((echo_duration * 0.343f) / 2.0f);

    return (dist_mm > 4000) ? 0 : dist_mm;
}

uint16_t measure_distance_mm(void) {
    uint16_t s1 = single_ping_mm();
    esp_rom_delay_us(5000);
    uint16_t s2 = single_ping_mm();
    esp_rom_delay_us(5000);
    uint16_t s3 = single_ping_mm();

    uint16_t samples[3] = {s1, s2, s3};
    std::sort(samples, samples + 3);
    return samples[1];
}

float read_ultrasonic_distance_cm(void) {
    uint16_t mm = measure_distance_mm();
    return (mm == 0) ? 0.0f : ((float)mm / 10.0f);
}

bool is_path_blocked_ahead(float threshold_cm) {
    uint16_t dist_mm = single_ping_mm();
    if (dist_mm == 0) return true; // Close contact / timeout blind spot
    float dist_cm = (float)dist_mm / 10.0f;
    return (dist_cm < threshold_cm);
}