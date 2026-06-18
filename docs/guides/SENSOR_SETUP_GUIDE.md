# 🚨 Sensor Setup Guide - GROVE-PIR & HLK-LD2451

## Overview

Your ESP32-S3 person detection system uses two sensors to **trigger** inference:

1. **GROVE PIR** - Motion sensor (detects movement)
2. **HLK-LD2451** - Millimeter-wave radar (detects presence/distance)

---

## Physical Connections

### GROVE-PIR Sensor

**GROVE PIR Characteristics:**
- Simple digital output (HIGH/LOW)
- Motion detection range: ~2-3 meters
- Output voltage: 0V (no motion) / 5V (motion detected)
- Response time: ~1-2 seconds

**Wiring:**
```
GROVE-PIR → ESP32-S3
━━━━━━━━━━━━━━━━━━━━━━
GND (Black)     → GND
VCC (Red)       → 5V (or 3.3V with resistor divider)
SIG (Yellow)    → GPIO21 (GROVE_PIR_PIN)
```

**⚠️ Important:** GROVE-PIR outputs 5V, but ESP32 GPIO is 3.3V tolerant
- **Option 1:** Use voltage divider (2x 10kΩ resistors):
  ```
  5V ──[R1 10k]──┬── GPIO21
                 │
              [R2 10k]
                 │
                GND
  ```
- **Option 2:** Use direct connection if your board has level shifting

---

### HLK-LD2451 Millimeter-Wave Radar

**HLK-LD2451 Characteristics:**
- Detection range: 0.75m - 6m (configurable)
- Communication: UART serial at 115200 baud
- Works through walls and clothing
- Provides distance + presence information

**Wiring:**
```
HLK-LD2451 → ESP32-S3
━━━━━━━━━━━━━━━━━━━━━━━━━━
GND (Black)     → GND
VCC (Red)       → 5V or 3.3V
TX (White/Out)  → GPIO14 (HLK_LD2451_RX_PIN)
RX (Green/In)   → GPIO12 (HLK_LD2451_TX_PIN)
```

**Serial Connection:**
- Baud rate: **115200**
- UART: **UART2** (Serial2 in Arduino)
- Data format: 8 data bits, 1 stop bit, no parity

---

## Sensor Detection Flow

```
┌─────────────────────────────────────────┐
│  SENSOR TRIGGER FLOW                    │
└─────────────────────────────────────────┘

Loop (runs every 10ms):
  1. Check GROVE-PIR
     ├─ If HIGH (motion) → Trigger inference
     └─ If LOW → Continue
     
  2. Check HLK-LD2451
     ├─ If data received → Parse distance
     ├─ If object < 6m → Trigger inference
     └─ Else → Continue
     
  3. Cooldown: Wait 2 seconds before next trigger
     └─ Prevents duplicate detections
     
  4. If triggered:
     ├─ Capture image from camera
     ├─ Run AI inference
     ├─ Send result to /run endpoint
     └─ Wait 2s cooldown
```

---

## API Endpoints for Sensors

### `/sensor_status` - Get real-time sensor state

**Request:**
```bash
curl http://172.20.10.2/sensor_status | jq
```

**Response:**
```json
{
  "pir": {
    "available": true,
    "last_state": 1,
    "description": "GROVE PIR motion sensor"
  },
  "hlk_ld2451": {
    "available": true,
    "detection_state": 0,
    "last_distance_mm": 1500,
    "description": "HLK-LD2451 millimeter-wave radar"
  }
}
```

**Fields:**
- `available` - Sensor initialized successfully
- `last_state` / `detection_state` - Current detection status (0=none, 1=detected)
- `last_distance_mm` - Last measured distance in millimeters

---

### `/health` - Overall system status

```bash
curl http://172.20.10.2/health | jq
```

---

## Troubleshooting

### Sensor Not Detected

**GROVE-PIR not working:**
```bash
curl http://172.20.10.2/sensor_status | jq '.pir'

# If available=false:
# 1. Check GPIO21 wiring
# 2. Check power voltage (5V or 3.3V)
# 3. Try reboot
```

**HLK-LD2451 not working:**
```bash
curl http://172.20.10.2/sensor_status | jq '.hlk_ld2451'

# If available=false:
# 1. Check GPIO12 (TX) and GPIO14 (RX) wiring
# 2. Verify 115200 baud rate
# 3. Check power connection
# 4. Reboot ESP32
```

---

### No Triggers (Sensors Not Triggering Inference)

**Check serial output:**
```bash
# Monitor ESP32 serial port
screen /dev/ttyUSB0 460800

# Look for:
# [PIR] Motion detected!
# [HLK] Detected target
# [TRIGGER] ...
```

**Causes:**
1. Sensors still available but not detecting anything (adjust positioning)
2. Camera not initialized (check /health endpoint)
3. Cooldown still active (2-second timeout between triggers)

---

### GROVE-PIR Always Triggered

**Issue:** PIR constantly shows motion even when nothing moving

**Solution:**
- Move PIR sensor away from heat sources
- Avoid direct sunlight
- Wait 1-2 minutes for sensor to stabilize after power-on
- Adjust detection threshold (if your PIR module has potentiometer)

---

### HLK-LD2451 Not Sending Data

**Issue:** HLK shows available but no data coming through

**Solution:**
1. Check UART connection (TX→RX, RX→TX cross-wiring)
2. Verify baud rate is 115200
3. Try to send command to HLK via serial monitor
4. Check if HLK requires initialization command

---

## Testing Sensors

### Manual PIR Test

```bash
# Open serial monitor at 460800 baud
screen /dev/ttyUSB0 460800

# Wave hand in front of PIR
# You should see in serial:
# [PIR] Motion detected!
# [TRIGGER] GROVE-PIR detected motion
# [FLOW] 🔴 SENSOR TRIGGERED
```

### Manual HLK Test

```bash
# Open serial monitor
screen /dev/ttyUSB0 460800

# Move object in front of HLK
# You should see:
# [HLK] Received: ...
# [TRIGGER] HLK-LD2451 detected target
# [FLOW] 🔴 SENSOR TRIGGERED
```

### API Test

```bash
# Check sensor status every second
while true; do
  curl http://172.20.10.2/sensor_status 2>/dev/null | jq '.'
  sleep 1
done
```

---

## Sensor Disconnection Handling ⚡

**If sensor unplugged:**
- Detection becomes unavailable: `"available": false`
- System continues running normally
- Endpoints still respond
- Camera still captures on demand
- **NO CRASH!** ✅

**If sensor reconnected:**
- Power cycle ESP32 once to re-initialize
- Sensor will be detected on reboot
- Automatic continuation

---

## Configuration Options

### Adjust Detection Sensitivity

**For GROVE-PIR:**
- Mounted on the sensor module (potentiometer)
- Turn clockwise: more sensitive
- Turn counter-clockwise: less sensitive

**For HLK-LD2451:**
- Configured via serial commands (if supported by your module)
- Edit firmware to change distance threshold:
  ```cpp
  // In shouldRunInference() function
  if (lidar_distance > 0 && lidar_distance < 2500) { // Change 2500 to desired mm
  ```

### Adjust Cooldown

Default: **2 seconds** between consecutive triggers

Edit in `firmware/main/main.ino`:
```cpp
const unsigned long INFERENCE_COOLDOWN_MS = 2000; // Change value here
```

---

## Pin Reference

| Component | Pin | GPIO | Function |
|-----------|-----|------|----------|
| GROVE-PIR | SIG | 21 | Digital input |
| HLK-LD2451 | RX | 14 | UART2 RX |
| HLK-LD2451 | TX | 12 | UART2 TX |
| Camera | PCLK | 13 | Parallel clock |
| External Wakeup | GPIO | 13 | External interrupt |

---

## Next Steps

1. **Connect sensors** following the wiring diagrams
2. **Power on ESP32** and observe boot messages
3. **Check /sensor_status** endpoint
4. **Test triggers** by moving in front of sensors
5. **Run inference** automatically on detection

---

## Error Codes

| Code | Meaning | Solution |
|------|---------|----------|
| `"available": false` | Sensor init failed | Check wiring, reboot |
| `last_state: 0` | No motion/presence | Move in front of sensor |
| `detection_state: 0` | No target detected | Object too far or wrong angle |

---

## Quick Reference

**Check everything:**
```bash
curl http://172.20.10.2/health && curl http://172.20.10.2/sensor_status
```

**Trigger inference manually (if sensors fail):**
```bash
curl http://172.20.10.2/run
```

**See what's happening:**
```bash
screen /dev/ttyUSB0 460800
```

---

## Support

If sensors not working:
1. Check `/sensor_status` endpoint
2. Check serial output (460800 baud)
3. Verify GPIO connections match config.h
4. Try power cycle (disconnect USB for 3 seconds)
5. Verify sensor power voltage (5V or 3.3V)

