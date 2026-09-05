# MQ-2 Gas Sensor - Warden Configuration

## ✅ Build Status
**SUCCESS** - Compiled without errors
- RAM Usage: 7.3% (23,964 / 327,680 bytes)
- Flash Usage: 23.5% (308,089 / 1,310,720 bytes)

## Pin Configuration (Verified against Warden Diagram)

| Pin | Connection | Type | Notes |
|-----|-----------|------|-------|
| **GPIO 34** | MQ-2 AOUT | Analog Input | ADC1_CH6, Input-only pin ✅ |
| **GPIO 35** | MQ-2 DOUT | Digital Input | Optional (not used in code) |
| **5V** | MQ-2 VCC | Power | **MUST be 5V** for heater |
| **GND** | MQ-2 GND | Ground | Common ground |

## ⚠️ Hardware Requirements

### Voltage Divider (CRITICAL)
Your MQ-2 outputs **5V** on AOUT, but ESP32 GPIO 34 can only handle **3.3V max**.

**Required voltage divider on AOUT line:**
```
MQ-2 AOUT (5V) ──┬── 10kΩ resistor ──┬── GPIO 34 (3.3V max)
                 │                    │
                 └─── 20kΩ resistor ──┴── GND
```
This scales 5V → 3.3V safely.

### Power Supply
- **MQ-2 VCC:** Must be 5V (NOT 3.3V)
- Heater requires ~300°C operating temperature
- 3.3V will NOT heat the sensor properly

## Current Configuration

```cpp
#define PIN_GAS_SENSOR 34        // MQ-2 AOUT (analog input)
#define GAS_THRESHOLD 1500       // ADC value (0-4095)
#define GAS_CHECK_INTERVAL 500   // Check every 500ms
```

## 🔧 Calibration Guide

### 1. Warm-up Period
- **Allow 30-60 seconds** after power-on for heater stabilization
- Readings before warm-up will be unreliable

### 2. Baseline Calibration (Clean Air)
1. Power on the robot in fresh air
2. Monitor serial output for 60 seconds
3. Note the baseline value (typically 300-800)

### 3. Threshold Adjustment
Current threshold: **1500** (default)

**Recommended thresholds based on your baseline:**
- If baseline = 400: Set threshold to **800-1000**
- If baseline = 600: Set threshold to **1200-1500**
- If baseline = 800: Set threshold to **1600-2000**

**Formula:** `THRESHOLD = BASELINE + (BASELINE * 1.5)`

### 4. Testing
1. Upload code and open Serial Monitor (115200 baud)
2. Wait 60 seconds for warm-up
3. Note baseline gas values in clean air
4. Introduce gas source (lighter, alcohol, etc.)
5. Verify detection alert triggers

## Serial Output Example

```
Gas Sensor Value: 450 / 4095
Gas Sensor Value: 460 / 4095
Gas Sensor Value: 1650 / 4095
⚠️  GAS DETECTED! ⚠️
Gas Level: 1650
Gas Sensor Value: 1820 / 4095 - ALERT!
```

## PPM Interpretation (from Warden Diagram)

| PPM Range | Status | Action |
|-----------|--------|--------|
| 0-599 | Safe | Normal operation |
| 600-799 | Warning | Monitor closely |
| 800+ | Danger | Alert / Stop robot |

**Note:** Raw ADC values (0-4095) need conversion to PPM. This requires calibration with known gas concentrations.

## Optional Enhancements

### 1. Stop Robot on Gas Detection
Uncomment this line in `checkGasLevel()`:
```cpp
// stopMotors();
```

### 2. Add GPIO 35 (DOUT) for Digital Confirmation
```cpp
#define PIN_GAS_DIGITAL 35
pinMode(PIN_GAS_DIGITAL, INPUT);
bool digitalGas = digitalRead(PIN_GAS_DIGITAL) == LOW; // Active LOW
```

### 3. Convert ADC to PPM
Requires calibration curve for your specific MQ-2 sensor:
```cpp
float adcToPPM(int adcValue) {
  // Example conversion (needs calibration)
  float voltage = adcValue * (3.3 / 4095.0);
  float ppm = /* your calibration formula */;
  return ppm;
}
```

## Troubleshooting

| Issue | Solution |
|-------|----------|
| Always reads 4095 | Check voltage divider, ensure AOUT < 3.3V |
| Always reads 0 | Check 5V power to MQ-2, verify connections |
| Erratic readings | Wait 60s warm-up, check ground connections |
| No gas detection | Lower GAS_THRESHOLD, verify gas type is detectable |

## Detectable Gases (MQ-2)
- LPG (Liquefied Petroleum Gas)
- Propane
- Methane
- Butane
- Hydrogen
- Alcohol
- Smoke

## Next Steps

1. ✅ **Build completed** - Code compiles successfully
2. ⚠️ **Install voltage divider** on AOUT line (GPIO 34)
3. 🔌 **Connect MQ-2** to 5V power (not 3.3V)
4. 📊 **Calibrate threshold** based on baseline readings
5. 🚀 **Upload and test** with Serial Monitor open

---
**Generated:** Warden Slave - MQ-2 Gas Detection System
**Verified against:** Warden Full Pin Diagram
