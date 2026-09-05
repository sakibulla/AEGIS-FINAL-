#pragma once

#include "esp_err.h"
#include "GuardianProtocol.h"

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t espnow_master_init(void);
esp_err_t espnow_master_send_command(guardian_command_t command);
esp_err_t espnow_master_sync_channel(uint8_t channel);

#ifdef __cplusplus
}
#endif