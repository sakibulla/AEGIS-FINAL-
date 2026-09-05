# Requirements Document

## Introduction

The AEGIS Guardian system currently implements ESP-NOW communication from a single master unit (Guardian) to multiple slave devices. This feature extends the system with **swarm communication** capabilities, enabling three master units (Guardian, Warden, and Pathfinder) to communicate with each other for coordinated security operations. The swarm will support synchronized alerts, status sharing, and coordinated behavioral responses across all master units before production deployment.

## Glossary

- **Swarm**: The collective network of three master units communicating via ESP-NOW
- **Guardian**: The primary master unit (existing codebase)
- **Warden**: The secondary master unit
- **Pathfinder**: The tertiary master unit
- **Master_Unit**: Any of the three ESP32-S3 devices (Guardian, Warden, or Pathfinder)
- **Swarm_Protocol**: The extended communication protocol supporting master-to-master messaging
- **Swarm_Manager**: The software component managing peer discovery, message routing, and swarm state
- **Heartbeat**: A periodic message indicating a Master_Unit is alive and operational
- **Broadcast_Message**: A message sent to all peers in the Swarm
- **Unicast_Message**: A message sent to a specific Master_Unit
- **Swarm_State**: The collective knowledge of which Master_Units are online and their status
- **Peer_Table**: The internal data structure tracking all known Master_Units
- **Home_Channel**: The WiFi channel used for ESP-NOW communication

## Requirements

### Requirement 1: Swarm Protocol Extension

**User Story:** As a system architect, I want to extend the existing Guardian protocol to support master-to-master communication, so that three master units can coordinate security operations.

#### Acceptance Criteria

1. THE Swarm_Protocol SHALL extend the existing guardian_message_t structure with a source_id field identifying the sender Master_Unit
2. THE Swarm_Protocol SHALL extend the existing guardian_message_t structure with a destination_id field supporting both unicast and broadcast addressing
3. THE Swarm_Protocol SHALL define message types for HEARTBEAT, ALERT_BROADCAST, STATUS_SYNC, and COMMAND_RELAY
4. THE Swarm_Protocol SHALL maintain backward compatibility with existing slave device communication
5. WHEN serializing a Swarm_Protocol message, THE Swarm_Manager SHALL validate the magic number is 0xAE61
6. WHEN deserializing a Swarm_Protocol message, THE Swarm_Manager SHALL reject messages with invalid magic numbers

### Requirement 2: Peer Discovery and Registration

**User Story:** As a Master_Unit, I want to discover and register other Master_Units in the Swarm, so that I can communicate with them.

#### Acceptance Criteria

1. WHEN the Swarm_Manager initializes, THE Swarm_Manager SHALL broadcast a discovery message on the Home_Channel
2. WHEN a Master_Unit receives a discovery message, THE Master_Unit SHALL respond with its identity and MAC address
3. THE Swarm_Manager SHALL maintain a Peer_Table with entries for Guardian, Warden, and Pathfinder
4. WHEN a new peer is discovered, THE Swarm_Manager SHALL add the peer to the ESP-NOW peer list with channel 0
5. THE Swarm_Manager SHALL store each peer's MAC address, Master_Unit identity, last_seen timestamp, and online status
6. WHEN a peer already exists in the Peer_Table, THE Swarm_Manager SHALL update the existing entry rather than create a duplicate

### Requirement 3: Heartbeat and Health Monitoring

**User Story:** As a Master_Unit, I want to monitor the health of other Master_Units in the Swarm, so that I know which units are operational.

#### Acceptance Criteria

1. THE Swarm_Manager SHALL broadcast a HEARTBEAT message every 5 seconds
2. WHEN a Master_Unit receives a HEARTBEAT message, THE Master_Unit SHALL update the sender's last_seen timestamp in the Peer_Table
3. WHEN a peer's last_seen timestamp exceeds 15 seconds, THE Swarm_Manager SHALL mark the peer as offline
4. WHEN a previously offline peer sends a HEARTBEAT message, THE Swarm_Manager SHALL mark the peer as online
5. THE Swarm_Manager SHALL log peer status transitions from online to offline and offline to online

### Requirement 4: Broadcast Alert Communication

**User Story:** As a Master_Unit detecting a security threat, I want to broadcast alerts to all other Master_Units in the Swarm, so that they can coordinate response.

#### Acceptance Criteria

1. WHEN a Master_Unit detects an object or intruder, THE Swarm_Manager SHALL broadcast an ALERT_BROADCAST message to all peers
2. THE ALERT_BROADCAST message SHALL include the alert type (object detection, face recognition, unknown person)
3. THE ALERT_BROADCAST message SHALL include a severity level (low, medium, high, critical)
4. THE ALERT_BROADCAST message SHALL include a timestamp of detection
5. WHEN a Master_Unit receives an ALERT_BROADCAST message, THE Swarm_Manager SHALL invoke a registered alert callback function
6. WHEN no alert callback is registered, THE Swarm_Manager SHALL log the received alert without processing

### Requirement 5: Unicast Command Relay

**User Story:** As a Master_Unit, I want to send commands to a specific Master_Unit in the Swarm, so that I can request coordinated actions.

#### Acceptance Criteria

1. WHEN sending a unicast message, THE Swarm_Manager SHALL verify the destination Master_Unit is online in the Peer_Table
2. WHEN the destination Master_Unit is offline, THE Swarm_Manager SHALL return an error code
3. WHEN the destination Master_Unit is online, THE Swarm_Manager SHALL send the message using esp_now_send with the destination MAC address
4. THE Swarm_Manager SHALL support sending CMD_NORMAL, CMD_STOP, and CMD_ALERT commands to specific Master_Units
5. WHEN a Master_Unit receives a unicast command, THE Swarm_Manager SHALL verify the destination_id matches its own identity
6. WHEN the destination_id does not match, THE Master_Unit SHALL ignore the message

### Requirement 6: Status Synchronization

**User Story:** As a Master_Unit, I want to share my operational status with other Master_Units, so that the Swarm maintains coordinated awareness.

#### Acceptance Criteria

1. THE Swarm_Manager SHALL broadcast a STATUS_SYNC message every 10 seconds
2. THE STATUS_SYNC message SHALL include the Master_Unit's current mode (idle, detecting, alert, voice_activation)
3. THE STATUS_SYNC message SHALL include camera status (enabled, disabled, error)
4. THE STATUS_SYNC message SHALL include WiFi signal strength (RSSI value)
5. WHEN a Master_Unit receives a STATUS_SYNC message, THE Swarm_Manager SHALL update the sender's status in the Peer_Table
6. THE Swarm_Manager SHALL provide a query function returning the current status of any Master_Unit in the Swarm

### Requirement 7: Thread-Safe Swarm Operations

**User Story:** As a system developer, I want Swarm operations to be thread-safe, so that multiple FreeRTOS tasks can interact with the Swarm_Manager without race conditions.

#### Acceptance Criteria

1. THE Swarm_Manager SHALL protect the Peer_Table with a FreeRTOS mutex
2. WHEN accessing the Peer_Table for read or write, THE Swarm_Manager SHALL acquire the mutex with a 100ms timeout
3. WHEN the mutex acquisition times out, THE Swarm_Manager SHALL return an error code
4. WHEN a Peer_Table operation completes, THE Swarm_Manager SHALL release the mutex
5. THE Swarm_Manager SHALL protect the message send queue with a FreeRTOS mutex
6. WHEN multiple tasks attempt to send messages simultaneously, THE Swarm_Manager SHALL serialize the sends without data corruption

### Requirement 8: Channel Synchronization for Swarm

**User Story:** As a Master_Unit, I want to ensure all Swarm peers use the same WiFi channel, so that ESP-NOW communication remains reliable.

#### Acceptance Criteria

1. WHEN the Swarm_Manager initializes, THE Swarm_Manager SHALL query the current Home_Channel from the WiFi driver
2. THE Swarm_Manager SHALL register all Swarm peers with channel 0 (inherit Home_Channel)
3. WHEN the WiFi channel changes, THE Swarm_Manager SHALL update all peer channel settings to channel 0
4. THE Swarm_Manager SHALL log the Home_Channel value during initialization
5. WHEN a peer registration fails with ESP_ERR_ESPNOW_CHAN, THE Swarm_Manager SHALL retry with channel 0

### Requirement 9: Backwards Compatibility with Slave Devices

**User Story:** As a system integrator, I want the Swarm implementation to maintain compatibility with existing slave devices, so that existing functionality continues to work.

#### Acceptance Criteria

1. THE Swarm_Manager SHALL maintain the existing slave device MAC address in the Peer_Table
2. WHEN sending commands to slave devices, THE Swarm_Manager SHALL use the existing guardian_command_t protocol
3. THE Swarm_Manager SHALL continue to support the existing espnow_master_send_command API for slave communication
4. THE Swarm_Manager SHALL not interfere with existing slave device send and receive callbacks
5. WHEN a message is received from a slave device, THE Swarm_Manager SHALL process it using the existing receive_callback
6. THE Swarm_Manager SHALL differentiate between master-to-master and master-to-slave messages based on protocol fields

### Requirement 10: Configurable Master Unit Identity

**User Story:** As a deployment engineer, I want to configure which Master_Unit identity the device uses, so that the same firmware can run on all three units.

#### Acceptance Criteria

1. THE Swarm_Manager SHALL read the Master_Unit identity from NVS storage during initialization
2. WHEN no identity exists in NVS, THE Swarm_Manager SHALL default to Guardian identity
3. THE Swarm_Manager SHALL provide a configuration function to set the Master_Unit identity to Guardian, Warden, or Pathfinder
4. WHEN the identity is set, THE Swarm_Manager SHALL persist the identity to NVS
5. THE Swarm_Manager SHALL log the configured Master_Unit identity during initialization
6. WHEN the identity configuration fails, THE Swarm_Manager SHALL continue with the default Guardian identity

### Requirement 11: Swarm Message Parsing and Pretty Printing

**User Story:** As a developer, I want to parse and print Swarm_Protocol messages for debugging and testing, so that I can verify protocol correctness.

#### Acceptance Criteria

1. THE Swarm_Manager SHALL provide a parse function that converts raw bytes into a Swarm_Protocol message structure
2. WHEN parsing a message with invalid magic number, THE parse function SHALL return a parsing error
3. WHEN parsing a message with invalid length, THE parse function SHALL return a parsing error
4. THE Swarm_Manager SHALL provide a pretty_print function that formats a Swarm_Protocol message into human-readable text
5. THE pretty_print function SHALL include all message fields (magic, source_id, destination_id, message type, sequence, payload)
6. FOR ALL valid Swarm_Protocol messages, parsing then pretty_printing then parsing SHALL produce an equivalent message structure (round-trip property)

### Requirement 12: FreeRTOS Task Integration

**User Story:** As a system developer, I want the Swarm_Manager to integrate with the existing FreeRTOS task architecture, so that it runs efficiently on the ESP32-S3.

#### Acceptance Criteria

1. THE Swarm_Manager SHALL create a swarm_task running on CPU 0 with priority 4
2. THE swarm_task SHALL handle periodic heartbeat transmission
3. THE swarm_task SHALL handle periodic status synchronization
4. THE swarm_task SHALL process incoming Swarm messages from a FreeRTOS queue
5. THE swarm_task SHALL implement a 100ms loop delay to prevent CPU starvation
6. WHEN the swarm_task is created, THE Swarm_Manager SHALL verify successful task creation and log any errors

### Requirement 13: Error Handling and Logging

**User Story:** As a system operator, I want comprehensive error handling and logging for Swarm operations, so that I can diagnose communication issues.

#### Acceptance Criteria

1. WHEN any ESP-NOW operation fails, THE Swarm_Manager SHALL log the error code and error name
2. THE Swarm_Manager SHALL log all peer discovery events with timestamp and MAC address
3. THE Swarm_Manager SHALL log all peer status transitions (online/offline) with timestamp
4. WHEN sending a message fails, THE Swarm_Manager SHALL log the destination, message type, and failure reason
5. THE Swarm_Manager SHALL use ESP_LOGI for informational messages, ESP_LOGW for warnings, and ESP_LOGE for errors
6. THE Swarm_Manager SHALL include the module tag "SWARM_MGR" in all log messages

### Requirement 14: Memory Efficiency

**User Story:** As a system architect, I want the Swarm_Manager to minimize memory usage, so that it fits within the ESP32-S3 resource constraints.

#### Acceptance Criteria

1. THE Peer_Table SHALL allocate space for exactly 4 peers (3 masters + 1 slave aggregate)
2. THE Swarm_Manager SHALL use stack allocation for message buffers in send functions
3. THE Swarm_Manager SHALL reuse the existing ESP-NOW send and receive buffers
4. THE swarm_task SHALL use a stack size of 4096 bytes
5. THE Swarm_Manager SHALL not allocate heap memory during message send or receive operations
6. WHEN the Swarm_Manager initializes, THE Swarm_Manager SHALL log the total memory allocated for Swarm operations

### Requirement 15: Configuration and Initialization API

**User Story:** As an application developer, I want a simple API to initialize and configure the Swarm_Manager, so that I can integrate it into main.cpp.

#### Acceptance Criteria

1. THE Swarm_Manager SHALL provide an espnow_swarm_init() function that initializes the Swarm subsystem
2. THE espnow_swarm_init() function SHALL return ESP_OK on success and an error code on failure
3. THE Swarm_Manager SHALL provide an espnow_swarm_start() function that starts the swarm_task
4. THE Swarm_Manager SHALL provide an espnow_swarm_register_alert_callback() function to register application alert handlers
5. THE Swarm_Manager SHALL provide an espnow_swarm_broadcast_alert() function for sending alerts to all peers
6. THE Swarm_Manager SHALL provide an espnow_swarm_send_command() function for sending unicast commands to specific Master_Units

