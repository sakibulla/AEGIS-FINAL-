#ifndef PROTOCOL_DEFS_H
#define PROTOCOL_DEFS_H

#include <stdint.h>
#include <stdbool.h>

#pragma pack(push, 1)

// ── 1. AI Vision Detections (Shared with Central Dashboard & AI Task) ─────
#define MAX_DETECTIONS 16

typedef struct {
    char     label[32];
    float    score;
    uint32_t x, y, w, h;    // Bounding box in camera/model space
} detection_t;

// ── 2. Message Type Enumeration ───────────────────────────────────────────
typedef enum {
    MSG_CMD_DRIVE     = 0x01,  // Continuous velocity control (linear/angular)
    MSG_CMD_SCAN      = 0x02,  // Trigger 180° servo sweep
    MSG_CMD_MOVE_STEP = 0x03,  // Discrete forward/turn movement step
    MSG_CMD_STOP      = 0x04,  // Emergency immediate motor stop
    
    MSG_SCAN_DATA     = 0x80,  // Slave -> Master 19-point ultrasonic response
    MSG_TELEMETRY_ACK = 0x81   // Acknowledgment frame
} msg_type_t;

// Movement & Trigger Command Aliases
#define CMD_STOP          MSG_CMD_STOP
#define CMD_DRIVE         MSG_CMD_DRIVE
#define CMD_SCAN          MSG_CMD_SCAN
#define CMD_FWD           MSG_CMD_MOVE_STEP
#define CMD_REV           0x05
#define CMD_LEFT          0x06
#define CMD_RIGHT         0x07

// ── 3. Intra-Bot Command Structures (Master S3-Eye -> Slave ESP32) ───────

// Continuous velocity command
typedef struct {
    uint8_t  msg_type;      // MSG_CMD_DRIVE
    uint8_t  target_id;     // 0x01 = Pathfinder, 0xFF = Broadcast
    int8_t   linear_vel;    // -100 to +100 (%)
    int8_t   angular_vel;   // -100 to +100 (%)
    uint32_t timestamp_ms;
} cmd_drive_t;

// Discrete step movement command
typedef struct {
    uint8_t  msg_type;      // MSG_CMD_MOVE_STEP
    uint8_t  target_id;     // 0x01 = Pathfinder
    int16_t  distance_mm;   // Forward (+) or reverse (-) in mm
    int16_t  turn_deg;      // Left (-) or right (+) in degrees
} cmd_move_step_t;

// Simple trigger command packet (e.g. MSG_CMD_SCAN, MSG_CMD_STOP)
typedef struct {
    uint8_t  msg_type;
    uint8_t  target_id;
} cmd_trigger_t;

// ── 4. Intra-Bot Telemetry Packet (Slave ESP32 -> Master S3-Eye) ──────────
typedef struct {
    uint8_t  msg_type;          // MSG_SCAN_DATA (0x80)
    uint8_t  sender_id;         // 0x01 = Pathfinder Slave
    float    pos_x;             // Estimated local X coordinate
    float    pos_y;             // Estimated local Y coordinate
    uint8_t  status_flags;      // Bit 0: Hardware OK, Bit 1: Obstacle Warning
    uint16_t distances_mm[19];  // 19 ultrasonic range readings (0° to 180° sweep)
} pathfinder_scan_packet_t;

// ── 5. Inter-Bot Swarm Mesh Packet (Pathfinder -> Guardian / Warden) ──────
typedef struct {
    uint8_t  type;         // 0: MAP_SNAP, 1: MAP_UPDATE, 2: DANGER, 3: HEARTBEAT
    uint8_t  totalSnaps;   // Target baseline snapshots (10 for Phase 1)
    uint8_t  snapIndex;    // Current snapshot index (0 to 9)
    float    x, y;         // Spatial map coordinate
    uint16_t d[19];        // 19 distance readings in mm
    bool     has_door;     // Visual/Geometric door tag
} MapPacket;               // Payload size = 52 bytes

// ── 6. Raw Buffer Container for Generic ESP-NOW Deserialization ───────────
typedef struct {
    uint8_t msg_type;
    uint8_t payload[240];
} espnow_raw_packet_t;

#pragma pack(pop)

#endif // PROTOCOL_DEFS_H