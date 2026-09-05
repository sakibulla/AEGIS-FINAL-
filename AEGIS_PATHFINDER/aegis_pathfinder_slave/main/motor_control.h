#ifndef MOTOR_CONTROL_H
#define MOTOR_CONTROL_H

#include <esp_err.h>
#include <stdint.h>

#define PIN_MOTOR_PWMA 25
#define PIN_MOTOR_AIN1 26
#define PIN_MOTOR_AIN2 27

#define PIN_MOTOR_PWMB 32
#define PIN_MOTOR_BIN1 33
#define PIN_MOTOR_BIN2 14

#define PIN_MOTOR_STBY 13

esp_err_t init_motor_controllers(void);
void drive_motors(int8_t linear, int8_t angular);
void execute_step_move(int16_t distance_mm, int16_t turn_deg);
void emergency_stop_motors(void);
void check_drive_watchdog(void);

// Autonomous 180° sweep & turn logic
int16_t scan_and_find_clearest_turn_angle(void);
void autonomous_obstacle_avoidance_turn(void);

#endif // MOTOR_CONTROL_H