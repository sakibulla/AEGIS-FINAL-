#pragma once
#include <Arduino.h>
#include <string.h>

// ============================================================
// GuardianProtocol.h
// Shared Master<->Slave command/telemetry definitions + framing.
// Keep this file IDENTICAL on both the Master and Slave projects.
// When ESP-NOW replaces UART later, only transportSend()/
// transportReceive() in main.cpp need to change — nothing here.
// ============================================================

// ---- Commands: Master -> Slave ----
enum SlaveCommandType : uint8_t {
  CMD_MOTOR_FWD        = 0x01,
  CMD_MOTOR_REV        = 0x02,
  CMD_MOTOR_STOP       = 0x03,
  CMD_MOTOR_TURN       = 0x04,
  CMD_SERVO_SWEEP      = 0x05,
  CMD_PAYLOAD_RELEASE  = 0x06
};

struct SlaveCommand {
  uint8_t  cmd;      // one of SlaveCommandType
  int16_t  param1;   // e.g. speed, angle
  int16_t  param2;   // reserved (e.g. duration)
};

// ---- Telemetry: Slave -> Master ----
enum SlaveTelemetryType : uint8_t {
  TEL_SENSOR_ULTRASONIC = 0x01,
  TEL_SENSOR_ACK        = 0x02,
  TEL_PAYLOAD_RELEASED  = 0x03
};

struct SlaveTelemetry {
  uint8_t  type;      // one of SlaveTelemetryType
  float    value;     // reading, or echoed cmd id for ACK
  uint32_t timestamp; // millis() at time of send
};

// ---- Framing: [START][LEN][PAYLOAD...][CHECKSUM] ----
// LEN = payload length only. CHECKSUM = XOR of LEN and every payload byte.
constexpr uint8_t FRAME_START_BYTE = 0xAA;
constexpr uint8_t FRAME_MAX_PAYLOAD = 16; // bigger than either struct, room to grow

inline uint8_t computeChecksum(uint8_t len, const uint8_t* payload) {
  uint8_t chk = len;
  for (uint8_t i = 0; i < len; i++) chk ^= payload[i];
  return chk;
}

// Encodes any POD struct into outBuf. Returns total frame length, or 0 on failure.
inline size_t encodeFrame(const uint8_t* payload, uint8_t len, uint8_t* outBuf, size_t outBufSize) {
  if (len > FRAME_MAX_PAYLOAD) return 0;
  if (outBufSize < (size_t)(len + 3)) return 0; // start + len + payload + checksum
  outBuf[0] = FRAME_START_BYTE;
  outBuf[1] = len;
  memcpy(&outBuf[2], payload, len);
  outBuf[2 + len] = computeChecksum(len, payload);
  return (size_t)len + 3;
}

inline size_t encodeCommand(const SlaveCommand& c, uint8_t* outBuf, size_t outBufSize) {
  return encodeFrame(reinterpret_cast<const uint8_t*>(&c), sizeof(SlaveCommand), outBuf, outBufSize);
}

inline size_t encodeTelemetry(const SlaveTelemetry& t, uint8_t* outBuf, size_t outBufSize) {
  return encodeFrame(reinterpret_cast<const uint8_t*>(&t), sizeof(SlaveTelemetry), outBuf, outBufSize);
}

// Byte-at-a-time decoder state machine. Feed it one byte per call; when it
// returns true, `out` has been filled with a validated T. Works for either
// SlaveCommand or SlaveTelemetry via the template parameter.
template <typename T>
class FrameDecoder {
public:
  bool feed(uint8_t byte, T& out) {
    switch (state_) {
      case WAIT_START:
        if (byte == FRAME_START_BYTE) state_ = WAIT_LEN;
        break;

      case WAIT_LEN:
        len_ = byte;
        idx_ = 0;
        if (len_ != sizeof(T) || len_ > FRAME_MAX_PAYLOAD) {
          state_ = WAIT_START; // malformed length, resync on next start byte
        } else {
          state_ = (len_ == 0) ? WAIT_CHECKSUM : WAIT_PAYLOAD;
        }
        break;

      case WAIT_PAYLOAD:
        buf_[idx_++] = byte;
        if (idx_ >= len_) state_ = WAIT_CHECKSUM;
        break;

      case WAIT_CHECKSUM: {
        uint8_t expected = computeChecksum(len_, buf_);
        state_ = WAIT_START;
        if (byte == expected) {
          memcpy(&out, buf_, sizeof(T));
          return true;
        }
        // checksum mismatch: drop frame, wait for next start byte
        break;
      }
    }
    return false;
  }

private:
  enum State { WAIT_START, WAIT_LEN, WAIT_PAYLOAD, WAIT_CHECKSUM } state_ = WAIT_START;
  uint8_t buf_[FRAME_MAX_PAYLOAD];
  uint8_t len_ = 0;
  uint8_t idx_ = 0;
};
