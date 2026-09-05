#pragma once

/*
 * AEGIS Swarm — Master-to-Master ESP-NOW protocol.
 *
 * Canonical copy. Byte-identical copies live in each of the three Masters'
 * main/ directories (Guardian, Pathfinder, Warden) — see SWARM_ESPNOW_DESIGN.md
 * at the repo root for the full design. Diff this file against the per-project
 * copies before each flash; there is no shared-component build linking them.
 */

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Fixed ESP-NOW channel for the swarm link, independent of the backend AP's
 * association state (SWARM_ESPNOW_DESIGN.md §2). */
#define AEGIS_SWARM_CHANNEL 6

typedef enum {
    SWARM_NODE_GUARDIAN   = 1,
    SWARM_NODE_PATHFINDER = 2,
    SWARM_NODE_WARDEN     = 3,
} swarm_node_id_t;

/* STA MACs, recorded off each physical board. Re-record and update here (and
 * in the two sibling copies) if a board is ever replaced. */
static const uint8_t SWARM_MAC_GUARDIAN[6]   = { 0xE8, 0xF6, 0x0A, 0xD6, 0xC8, 0xE8 };  /* E8:F6:0A:D6:C8:E8 */
static const uint8_t SWARM_MAC_PATHFINDER[6] = { 0xE8, 0xF6, 0x0A, 0xD7, 0x91, 0x60 };  /* E8:F6:0A:D7:91:60 */
static const uint8_t SWARM_MAC_WARDEN[6]     = { 0x94, 0xA9, 0x90, 0x0A, 0xEF, 0x6C };  /* 94:A9:90:0A:EF:6C */

#define SWARM_MAGIC 0xA5C1   /* distinguishes swarm packets from this bot's own Master<->Slave traffic */

typedef enum {
    SWARM_MSG_HEARTBEAT      = 0x01,  /* periodic liveness, all bots, broadcast */
    SWARM_MSG_WAKE            = 0x02, /* Guardian   -> swarm: "Hi ESP" fired, everyone wake up */
    SWARM_MSG_FIRE_ALERT      = 0x03, /* Warden     -> swarm: fire/smoke state change */
    SWARM_MSG_SECURITY_ALERT  = 0x04, /* Guardian   -> swarm: intruder / dangerous object */
    SWARM_MSG_MAP_UPDATE      = 0x05, /* Pathfinder -> swarm: door/map snapshot */
    SWARM_MSG_ALL_CLEAR       = 0x06, /* any bot    -> swarm: its own alert condition cleared */
} swarm_msg_type_t;

#pragma pack(push, 1)
typedef struct {
    uint16_t magic;        /* SWARM_MAGIC, always first two bytes -> demux key in recv_cb */
    uint8_t  type;         /* swarm_msg_type_t */
    uint8_t  source;       /* swarm_node_id_t */
    uint32_t sequence;     /* monotonically increasing per-sender, for dedup / drop detection */
    uint32_t uptime_ms;    /* sender's uptime, doubles as a coarse heartbeat timestamp */

    union {
        struct {                       /* SWARM_MSG_FIRE_ALERT */
            uint8_t  state;            /* mirrors warden_command_t: 0 clear,1 fire,2 smoke,3 both */
            uint16_t fire_confidence_x1000;
            uint16_t smoke_confidence_x1000;
        } fire;

        struct {                       /* SWARM_MSG_SECURITY_ALERT */
            uint8_t  is_intruder;
            uint8_t  is_dangerous_object;
            float    confidence;
            char     label[24];        /* e.g. object label, "Intruder" */
        } security;

        struct {                       /* SWARM_MSG_MAP_UPDATE */
            float    x, y;             /* spatial coordinate (matches existing MapPacket) */
            bool     has_door;
            uint8_t  snap_index;
            uint8_t  total_snaps;
        } map;

        uint8_t raw[24];               /* headroom / future fields, keeps struct size stable */
    } payload;
} swarm_packet_t;   /* fixed size well under ESP-NOW's 250-byte cap */
#pragma pack(pop)

#ifdef __cplusplus
}
#endif
