#include "motor_control.h"
#include "ultrasonic_sensor.h"
#include "servo_sweep.h"
#include <driver/gpio.h>
#include <driver/ledc.h>
#include <esp_timer.h>
#include <esp_log.h>
#include <cstdlib>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

static const char *TAG = "MOTOR_CTRL";

#define PWM_FREQ_HZ          5000
#define PWM_RESOLUTION       LEDC_TIMER_8_BIT
#define WATCHDOG_TIMEOUT_US  500000
#define MIN_SAFETY_DIST_CM   20.0f // Obstacle detection threshold

static int64_t s_last_cmd_time_us = 0;

esp_err_t init_motor_controllers(void) {
    gpio_config_t io_conf = {};
    io_conf.pin_bit_mask = (1ULL << PIN_MOTOR_AIN1) | (1ULL << PIN_MOTOR_AIN2) |
                           (1ULL << PIN_MOTOR_BIN1) | (1ULL << PIN_MOTOR_BIN2) |
                           (1ULL << PIN_MOTOR_STBY);
    io_conf.mode = GPIO_MODE_OUTPUT;
    io_conf.pull_up_en = GPIO_PULLUP_DISABLE;
    io_conf.pull_down_en = GPIO_PULLDOWN_ENABLE;
    io_conf.intr_type = GPIO_INTR_DISABLE;
    gpio_config(&io_conf);

    gpio_set_level((gpio_num_t)PIN_MOTOR_STBY, 1);

    ledc_timer_config_t timer_conf = {};
    timer_conf.speed_mode      = LEDC_LOW_SPEED_MODE;
    timer_conf.duty_resolution = PWM_RESOLUTION;
    timer_conf.timer_num       = LEDC_TIMER_0;
    timer_conf.freq_hz         = PWM_FREQ_HZ;
    timer_conf.clk_cfg         = LEDC_AUTO_CLK;
    ledc_timer_config(&timer_conf);

    ledc_channel_config_t chan_a = {};
    chan_a.gpio_num   = PIN_MOTOR_PWMA;
    chan_a.speed_mode = LEDC_LOW_SPEED_MODE;
    chan_a.channel    = LEDC_CHANNEL_0;
    chan_a.intr_type  = LEDC_INTR_DISABLE;
    chan_a.timer_sel  = LEDC_TIMER_0;
    chan_a.duty       = 0;
    chan_a.hpoint     = 0;
    ledc_channel_config(&chan_a);

    ledc_channel_config_t chan_b = {};
    chan_b.gpio_num   = PIN_MOTOR_PWMB;
    chan_b.speed_mode = LEDC_LOW_SPEED_MODE;
    chan_b.channel    = LEDC_CHANNEL_1;
    chan_b.intr_type  = LEDC_INTR_DISABLE;
    chan_b.timer_sel  = LEDC_TIMER_0;
    chan_b.duty       = 0;
    chan_b.hpoint     = 0;
    ledc_channel_config(&chan_b);

    emergency_stop_motors();
    ESP_LOGI(TAG, "TB6612 Motor Controller active with 180° Sweep Navigation.");
    return ESP_OK;
}

void emergency_stop_motors(void) {
    gpio_set_level((gpio_num_t)PIN_MOTOR_AIN1, 0);
    gpio_set_level((gpio_num_t)PIN_MOTOR_AIN2, 0);
    gpio_set_level((gpio_num_t)PIN_MOTOR_BIN1, 0);
    gpio_set_level((gpio_num_t)PIN_MOTOR_BIN2, 0);

    ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, 0);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0);
    ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_1, 0);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_1);
}

/**
 * @brief Performs a 180° servo sweep (7 sectors: 0°, 30°, 60°, 90°, 120°, 150°, 180°),
 *        finds the direction with the greatest open clearance, and returns the relative turn angle.
 * 
 * @return int16_t Relative turn angle (-90° to +90°). Returns +180° if all directions are blocked.
 */
int16_t scan_and_find_clearest_turn_angle(void) {
    emergency_stop_motors();
    ESP_LOGI(TAG, "Starting 180° Servo Obstacle Avoidance Sweep...");

    const uint8_t angles[7] = {0, 30, 60, 90, 120, 150, 180};
    uint16_t max_clearance_mm = 0;
    uint8_t best_angle = 90;

    for (int i = 0; i < 7; i++) {
        set_servo_angle(angles[i]);
        vTaskDelay(pdMS_TO_TICKS(50)); // Allow servo to reach target position

        uint16_t dist = measure_distance_mm();
        ESP_LOGI(TAG, " ├─ Angle %3d° | Measured Clearance: %u mm", angles[i], dist);

        if (dist > max_clearance_mm) {
            max_clearance_mm = dist;
            best_angle = angles[i];
        }
    }

    set_servo_angle(90); // Reset sensor head to forward center

    // If clearest direction has less than 20cm clearance, turn completely around
    if (max_clearance_mm < (uint16_t)(MIN_SAFETY_DIST_CM * 10.0f)) {
        ESP_LOGW(TAG, "All sectors blocked (<20cm)! Executing 180° turn-around.");
        return 180;
    }

    // Convert absolute servo angle (0..180) to relative bot turn degrees (-90 to +90)
    // Servo 0° (Right) -> -90° Turn | Servo 90° (Center) -> 0° Turn | Servo 180° (Left) -> +90° Turn
    int16_t relative_turn_deg = (int16_t)best_angle - 90;
    ESP_LOGI(TAG, "Clearest path found at Servo %d° -> Executing relative turn of %d°", best_angle, relative_turn_deg);

    return relative_turn_deg;
}

void autonomous_obstacle_avoidance_turn(void) {
    int16_t turn_deg = scan_and_find_clearest_turn_angle();
    if (turn_deg != 0) {
        int8_t rot_dir = (turn_deg > 0) ? 60 : -60;
        uint32_t turn_duration_ms = (uint32_t)(std::abs(turn_deg) * 8);
        
        drive_motors(0, rot_dir);
        vTaskDelay(pdMS_TO_TICKS(turn_duration_ms));
        emergency_stop_motors();
    }
}

void drive_motors(int8_t linear, int8_t angular) {
    s_last_cmd_time_us = esp_timer_get_time();
    gpio_set_level((gpio_num_t)PIN_MOTOR_STBY, 1);

    // Forward motion obstacle interlock
    if (linear > 0) {
        set_servo_angle(90);

        if (is_path_blocked_ahead(MIN_SAFETY_DIST_CM)) {
            ESP_LOGW(TAG, "OBSTACLE DETECTED! Stopping and sweeping for clear path...");
            autonomous_obstacle_avoidance_turn();
            return;
        }
    }

    int left_speed  = linear + angular;
    int right_speed = linear - angular;

    if (left_speed > 100)   left_speed = 100;
    if (left_speed < -100)  left_speed = -100;
    if (right_speed > 100)  right_speed = 100;
    if (right_speed < -100) right_speed = -100;

    if (left_speed >= 0) {
        gpio_set_level((gpio_num_t)PIN_MOTOR_AIN1, 1);
        gpio_set_level((gpio_num_t)PIN_MOTOR_AIN2, 0);
    } else {
        gpio_set_level((gpio_num_t)PIN_MOTOR_AIN1, 0);
        gpio_set_level((gpio_num_t)PIN_MOTOR_AIN2, 1);
        left_speed = -left_speed;
    }

    if (right_speed >= 0) {
        gpio_set_level((gpio_num_t)PIN_MOTOR_BIN1, 1);
        gpio_set_level((gpio_num_t)PIN_MOTOR_BIN2, 0);
    } else {
        gpio_set_level((gpio_num_t)PIN_MOTOR_BIN1, 0);
        gpio_set_level((gpio_num_t)PIN_MOTOR_BIN2, 1);
        right_speed = -right_speed;
    }

    uint32_t duty_a = (uint32_t)((left_speed * 255) / 100);
    uint32_t duty_b = (uint32_t)((right_speed * 255) / 100);

    ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, duty_a);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0);

    ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_1, duty_b);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_1);
}

void execute_step_move(int16_t distance_mm, int16_t turn_deg) {
    if (turn_deg != 0) {
        int8_t rot_dir = (turn_deg > 0) ? 60 : -60;
        uint32_t turn_duration_ms = (uint32_t)(std::abs(turn_deg) * 8);
        drive_motors(0, rot_dir);
        vTaskDelay(pdMS_TO_TICKS(turn_duration_ms));
    }

    if (distance_mm != 0) {
        if (distance_mm > 0) {
            set_servo_angle(90);
        }

        int8_t speed = (distance_mm > 0) ? 70 : -70;
        uint32_t drive_duration_ms = (uint32_t)(std::abs(distance_mm) * 2.2f);
        
        drive_motors(speed, 0);

        uint32_t elapsed_ms = 0;
        while (elapsed_ms < drive_duration_ms) {
            if (distance_mm > 0) {
                if (is_path_blocked_ahead(MIN_SAFETY_DIST_CM)) {
                    ESP_LOGE(TAG, "EMERGENCY BRAKE: Obstacle mid-step! Rerouting...");
                    autonomous_obstacle_avoidance_turn();
                    return;
                }
            }
            vTaskDelay(pdMS_TO_TICKS(20));
            elapsed_ms += 20;
        }
    }

    emergency_stop_motors();
}

void check_drive_watchdog(void) {
    if (s_last_cmd_time_us > 0 && (esp_timer_get_time() - s_last_cmd_time_us) > WATCHDOG_TIMEOUT_US) {
        emergency_stop_motors();
        s_last_cmd_time_us = 0;
    }
}