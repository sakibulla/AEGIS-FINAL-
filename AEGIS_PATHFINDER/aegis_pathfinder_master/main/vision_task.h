#ifndef VISION_TASK_H
#define VISION_TASK_H

#include <esp_err.h>
#include <esp_camera.h>
#include "protocol_defs.h"

esp_err_t init_hardware_camera(void);
void start_vision_task(void);
void get_current_detections(detection_t *out_dets, int *out_count, int *out_ms);
bool is_door_currently_detected(void);

// Thread-safe camera wrapper declarations
camera_fb_t* safe_camera_fb_get(void);
void safe_camera_fb_return(camera_fb_t *fb);

#endif // VISION_TASK_H