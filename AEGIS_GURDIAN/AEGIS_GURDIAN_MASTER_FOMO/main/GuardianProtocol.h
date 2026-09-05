#pragma once

#include <stdint.h>

/* =========================================================
 * AEGIS GUARDIAN ESP-NOW PROTOCOL
 * ========================================================= */

#define GUARDIAN_PROTOCOL_MAGIC 0xAE61

typedef enum {
    CMD_NORMAL = 0,
    CMD_STOP   = 1,
    CMD_ALERT  = 2
} guardian_command_t;

typedef struct {
    uint16_t magic;
    uint8_t command;
    uint8_t reserved;
    uint32_t sequence;
} guardian_message_t;