#pragma once

#include "esp_err.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t voice_activation_init(void);
esp_err_t voice_activation_start(void);
bool      voice_activation_wait(uint32_t timeout_ms);
void      voice_activation_stop(void);

#ifdef __cplusplus
}
#endif