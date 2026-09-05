#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <esp_log.h>
#include "motor_control.h"
#include "servo_sweep.h"
#include "ultrasonic_sensor.h"
#include "espnow_slave.h"

static const char *TAG = "AEGIS_SWARM_SLAVE";

extern "C" void app_main(void) {
    ESP_LOGI(TAG, "Booting A.E.G.I.S. Swarm Slave Node...");

    // 1. Initialize hardware drivers
    init_motor_controllers();
    init_servo_sweep();
    init_ultrasonic_sensor();

    // 2. Initialize ESP-NOW communications
    init_slave_espnow();

    ESP_LOGI(TAG, "Swarm Slave fully operational. Awaiting Master commands.");

    // Background loop monitoring watchdog execution
    while (1) {
        check_drive_watchdog();
        vTaskDelay(pdMS_TO_TICKS(30));
    }
}