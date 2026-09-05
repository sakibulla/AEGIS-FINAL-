# A.E.G.I.S. WARDEN MASTER

ESP32-S3-EYE + OV2640 + your supplied Edge Impulse AEGIS_WARDEN FOMO model.

## Current stage

This package is MASTER ONLY.

It:
- initializes the OV2640 camera
- captures 96x96 RGB565 frames
- converts pixels to Edge Impulse 0xRRGGBB signal format
- runs your FOMO fire/smoke model
- applies a 0.50 confidence threshold
- requires 3 consecutive detection frames
- requires 8 clear frames before CLEAR
- initializes ESP-NOW without connecting to an AP
- broadcasts Warden packets during development
- prints ESP-NOW send status

The Slave is NOT required to test the Master AI pipeline.

## Important correction

The Edge Impulse signal for an image is one packed 0xRRGGBB value per pixel. Therefore `get_data()` uses one signal sample per pixel, not one sample per RGB channel.

The supplied model metadata reports:
- 96x96 input
- 9216 raw samples
- object detection/FOMO
- labels fire and smoke
- largest TFLite arena about 185 KB

## Hardware

Camera pins used by this project:

- XCLK 15
- SIOD 4
- SIOC 5
- D0 11
- D1 9
- D2 8
- D3 10
- D4 12
- D5 18
- D6 17
- D7 16
- VSYNC 6
- HREF 7
- PCLK 13

These correspond to the common ESP32-S3-EYE/S3-EYE-style OV2640 mapping.

## Build

Use the ESP-IDF version that matches your installed Edge Impulse/ESP32-Camera components. For your previous setup, ESP-IDF 5.4.x is the preferred first target.

```bash
cd ~/esp/esp-idf
. ./export.sh

cd /path/to/AEGIS_WARDEN_MASTER

idf.py set-target esp32s3
idf.py reconfigure
idf.py build
```

Flash:

```bash
idf.py -p /dev/ttyACM0 flash monitor
```

Change `/dev/ttyACM0` if your board appears on another port.

## Expected startup

```text
A.E.G.I.S. WARDEN MASTER
FIRE + SMOKE AI NODE
Project: AEGIS_WARDEN
Input: 96x96 RGB
Raw samples: 9216
TFLite arena: 185036 bytes
OV2640 initialized: 96x96 RGB565
Warden STA MAC: XX:XX:XX:XX:XX:XX
ESP-NOW channel: 1
ESP-NOW target: BROADCAST
Warden Master ready.
Watching for FIRE / SMOKE...
```

Then:

```text
Inference: DSP=... NN=... POST=... | FIRE=0.00 SMOKE=0.00
Inference: DSP=... NN=... POST=... | FIRE=0.78 SMOKE=0.00
Inference: DSP=... NN=... POST=... | FIRE=0.84 SMOKE=0.00
Inference: DSP=... NN=... POST=... | FIRE=0.81 SMOKE=0.00
TX -> FIRE seq=1 fire=0.81 smoke=0.00
ESP-NOW send status: SUCCESS
```

## Later: connect the real Slave

Change:

```cpp
#define WARDEN_USE_BROADCAST 1
```

to:

```cpp
#define WARDEN_USE_BROADCAST 0
```

Then replace:

```cpp
#define WARDEN_SLAVE_MAC { 0x24, 0x6F, 0x28, 0xAA, 0xBB, 0xCC }
```

with the actual Slave MAC.

Both devices must use the same:

```cpp
#define WARDEN_ESPNOW_CHANNEL 1
```

Do not connect Warden to a Wi-Fi AP during this decentralized ESP-NOW setup.

## Detection behavior

FIRE:
3 consecutive frames >= 0.50

SMOKE:
3 consecutive frames >= 0.50

FIRE+SMOKE:
both conditions confirmed

CLEAR:
8 consecutive frames with neither class >= 0.50

Active alerts are retransmitted every 3 seconds.

## Notes

ESP-NOW send callback runs in the Wi-Fi task. Keep it short; this implementation only logs the status. If we later need guaranteed application delivery, add an ACK/retry state machine in a normal task.
