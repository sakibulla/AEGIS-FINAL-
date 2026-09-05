#pragma once

// ============================================================
// A.E.G.I.S. WARDEN MASTER
// ESP32-S3-EYE + OV2640 + Edge Impulse FOMO
// FIRE / SMOKE detection + ESP-NOW transmitter
// ============================================================

// 1 = broadcast while developing Master.
// 0 = unicast to the Slave MAC below.
#define WARDEN_USE_BROADCAST 0

#define WARDEN_SLAVE_MAC { \
    0x00, 0x70, 0x07, 0x7E, 0xD8, 0x14 \
}

// Warden and Slave must use the same Wi-Fi channel.
#define WARDEN_ESPNOW_CHANNEL 1

// FOMO confidence threshold.
#define WARDEN_DETECTION_THRESHOLD 0.50f

// Consecutive frames required before an alert.
#define WARDEN_DETECTION_FRAMES 3

// Consecutive clear frames required before CLEAR.
#define WARDEN_CLEAR_FRAMES 8

// Repeat an active command after this interval.
#define WARDEN_ALERT_REPEAT_MS 3000

// Delay between inference cycles (allow time for camera streaming)
// Set to 10000 (10 seconds) to prioritize streaming, or 0 to disable AI
#define WARDEN_FRAME_DELAY_MS 10000
