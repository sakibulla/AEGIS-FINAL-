#ifndef SERVO_SWEEP_H
#define SERVO_SWEEP_H

#include <esp_err.h>
#include <stdint.h>

// Exact Pin Assignment from Pin Diagram: GPIO 15
#ifndef PIN_SERVO_SIG
#define PIN_SERVO_SIG 15
#endif

esp_err_t init_servo_sweep(void);
void set_servo_angle(uint8_t angle_deg);

#endif // SERVO_SWEEP_H