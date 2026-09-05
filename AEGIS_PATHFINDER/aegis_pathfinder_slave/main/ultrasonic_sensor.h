#ifndef ULTRASONIC_SENSOR_H
#define ULTRASONIC_SENSOR_H

#include <esp_err.h>
#include <stdint.h>
#include <stdbool.h>

// Exact Pin Assignment from Pin Diagram: TRIG=21, ECHO=22
#ifndef PIN_HCSR04_TRIG
#define PIN_HCSR04_TRIG 21
#endif

#ifndef PIN_HCSR04_ECHO
#define PIN_HCSR04_ECHO 22
#endif

esp_err_t init_ultrasonic_sensor(void);
uint16_t measure_distance_mm(void);
float read_ultrasonic_distance_cm(void);
bool is_path_blocked_ahead(float threshold_cm);

#endif // ULTRASONIC_SENSOR_H