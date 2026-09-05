#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum : uint8_t
{
    WARDEN_CMD_CLEAR = 0,
    WARDEN_CMD_FIRE = 1,
    WARDEN_CMD_SMOKE = 2,
    WARDEN_CMD_FIRE_SMOKE = 3

} warden_command_t;


/*
 * WARDEN ESP-NOW PACKET
 *
 * MUST BE IDENTICAL ON MASTER AND SLAVE.
 *
 * Total size:
 *
 * version                  1
 * source                   1
 * command                  1
 * reserved                 1
 * sequence                 2
 * fire confidence          2
 * smoke confidence         2
 *
 * TOTAL = 10 BYTES
 */

typedef struct __attribute__((packed))
{
    uint8_t  version;
    uint8_t  source;
    uint8_t  command;
    uint8_t  reserved;

    uint16_t sequence;

    uint16_t fire_confidence_x1000;
    uint16_t smoke_confidence_x1000;

} warden_packet_t;


/*
 * Protocol version
 */
#define WARDEN_PROTOCOL_VERSION 1


/*
 * MASTER source ID
 *
 * The ESP32-S3 Warden Master sends packets
 * with source = 2.
 */
#define WARDEN_SOURCE_ID 2


/*
 * WARDEN SLAVE -> MASTER STATUS PACKET
 *
 * Separate from warden_packet_t above (Master -> Slave only). Carries the
 * Slave's own sensor telemetry — currently just its gas sensor — back up to
 * the Master, which previously had no way to receive anything from its Slave
 * at all (send-only link).
 *
 * Distinguished on the wire by `magic` up front: warden_packet_t has no
 * magic (its first byte is always WARDEN_PROTOCOL_VERSION == 1), and the
 * swarm protocol (aegis_swarm_protocol.h) uses a different magic
 * (SWARM_MAGIC == 0xA5C1), so there's no collision between the three.
 *
 * MUST BE IDENTICAL ON MASTER AND SLAVE.
 */

#define WARDEN_SLAVE_STATUS_MAGIC 0xA5D2
#define WARDEN_SLAVE_STATUS_VERSION 1

/*
 * Source ID of the Warden slave, used only in warden_slave_status_t.
 * Distinct from WARDEN_SOURCE_ID above, which identifies the Master in the
 * existing Master -> Slave direction.
 */
#define WARDEN_SLAVE_ID 1

typedef struct __attribute__((packed))
{
    uint16_t magic;          // WARDEN_SLAVE_STATUS_MAGIC
    uint8_t  version;        // WARDEN_SLAVE_STATUS_VERSION
    uint8_t  source;         // WARDEN_SLAVE_ID
    uint16_t sequence;       // monotonically increasing, for dedup / drop detection
    uint8_t  gas_detected;   // 0 = clear, 1 = detected
    uint16_t gas_value;      // raw ADC reading (0-4095)

} warden_slave_status_t;


#ifdef __cplusplus
}
#endif