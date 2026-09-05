#ifndef ESPNOW_MESH_H
#define ESPNOW_MESH_H

#include <esp_err.h>
#include <stdbool.h>
#include <stdint.h>
#include "protocol_defs.h"

// Alias for command type compatibility
typedef uint8_t SlaveCmdType;

esp_err_t init_master_espnow(uint8_t wifi_channel);
esp_err_t send_cmd_to_slave(uint8_t cmd, uint16_t duration_or_dist);
bool wait_for_slave_telemetry(uint16_t out_distances[19], uint32_t timeout_ms);
esp_err_t broadcast_map_packet(const MapPacket *pkt);

#endif // ESPNOW_MESH_H