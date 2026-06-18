# Efficient Development Workflow Strategy

## 🎯 Core Philosophy

**Minimize compilation cycles. Maximize testing iterations.**

The ESP32 firmware compilation takes 15-20 minutes with a clean cache. Instead of rebuilding everything each time, we separate:
- **Firmware Changes** (logic, sensors, APIs) → Full recompile required
- **Python Client Changes** (data processing, UI, HTTP calls) → No recompile needed
- **Config Changes** (WiFi SSID, GPIO pins, timeouts) → Minimal recompile

This enables:
- **Python developers** to test new client scripts without touching firmware
- **Firmware developers** to test hardware changes with pre-compiled binaries
- **Rapid iteration** from 15 minutes/test down to 30 seconds/test

---

## 📋 Development Scenarios

### Scenario 1: Testing a New Python Client Script
**Goal:** Test person_detector.py or other HTTP API client  
**Time:** 30 seconds per test  
**Frequency:** Very high (dozens of tests)

```bash
# 1. Upload pre-compiled binary (ONCE per firmware version)
~/.arduino15/packages/esp32/tools/esptool_py/5.2.0/esptool \
  --port /dev/ttyUSB0 --baud 460800 write-flash 0x0 \
  firmware/compiled_binaries/main_firmware/main.ino.merged.bin

# 2. Edit your Python client
nano python/main_client.py

# 3. Test immediately
python3 python/main_client.py

# 4. Iterate: Change Python code → Test (no firmware reupload needed)
# Repeat steps 2-3 as many times as needed
```

**Key Point:** Firmware doesn't change, only Python client. Skip reupload until firmware change needed.

---

### Scenario 2: Changing Hardware Behavior
**Goal:** Add sensor reading, modify sensor thresholds, change GPIO pins  
**Time:** 15-20 minutes (compilation) + 30 seconds (upload)  
**Frequency:** Low (maybe 2-3 times per session)

```bash
# 1. Edit firmware (logic, sensor code, GPIO configuration)
nano firmware/main/main.ino
# Example: Change GROVE_PIR_PIN from 21 to 19
# Example: Modify shouldRunInference() logic
# Example: Add new HTTP endpoint

# 2. Clean cache to force full recompile
rm -rf ~/.cache/arduino/sketches/*

# 3. Compile (takes 15-20 minutes - get coffee ☕)
FQBN="esp32:esp32:esp32s3:PartitionScheme=huge_app"
arduino-cli compile --fqbn $FQBN firmware/main/main.ino 2>&1 | tee compile.log

# 4. Extract sketch ID from log
SKETCH_ID=$(grep -o "sketches/[A-F0-9]*" compile.log | cut -d/ -f2 | head -1)
CACHE_DIR="~/.cache/arduino/sketches/$SKETCH_ID"

# 5. Update repo binaries (so others can use them without recompiling)
cp $CACHE_DIR/main.ino.* firmware/compiled_binaries/main_firmware/

# 6. Upload new firmware (30 seconds)
~/.arduino15/packages/esp32/tools/esptool_py/5.2.0/esptool \
  --port /dev/ttyUSB0 --baud 460800 write-flash 0x0 \
  firmware/compiled_binaries/main_firmware/main.ino.merged.bin

# 7. Wait for boot
sleep 5

# 8. Test with Python client
python3 python/main_client.py

# 9. If issues found, iterate:
#    - Small fix? → Full recompile (step 3-8)
#    - Python testing? → Skip to step 8
```

---

### Scenario 3: Quick Config Changes
**Goal:** Change WiFi SSID, adjust sensor thresholds (as constants), modify timeouts  
**Time:** 1-2 minutes + 30 seconds upload  
**Frequency:** Medium (during setup/calibration)

```bash
# 1. Edit ONLY configuration constants (lines 20-50 in main.ino)
nano firmware/main/main.ino
# Examples of config-only changes:
# - WIFI_SSID, WIFI_PASSWORD
# - GROVE_PIR_PIN, HLK_UART_RX/TX_PIN
# - SENSOR_COOLDOWN_MS, INFERENCE_INTERVAL_MS
# - WiFi/Sensor error thresholds

# 2. Quick recompile (Arduino caches most of the build)
FQBN="esp32:esp32:esp32s3:PartitionScheme=huge_app"
arduino-cli compile --fqbn $FQBN firmware/main/main.ino 2>&1 | tee compile.log

# 3. Extract and upload
SKETCH_ID=$(grep -o "sketches/[A-F0-9]*" compile.log | cut -d/ -f2 | head -1)
cp ~/.cache/arduino/sketches/$SKETCH_ID/main.ino.* \
   firmware/compiled_binaries/main_firmware/

~/.arduino15/packages/esp32/tools/esptool_py/5.2.0/esptool \
  --port /dev/ttyUSB0 --baud 460800 write-flash 0x0 \
  firmware/compiled_binaries/main_firmware/main.ino.merged.bin

# 4. Test
sleep 5
python3 python/main_client.py
```

---

## 🔄 Recommended Team Workflow

### Role 1: Firmware Developer (Laptop A - Arduino IDE)
- Responsible for sensor integration, hardware logic, API endpoints
- Every 1-2 hours: Push compiled binaries to repo after testing
- Document changes in CHANGELOG.md

```bash
# Regular cycle:
# 1. Edit firmware/main/main.ino
# 2. Compile (15 min) → Upload (30 sec) → Test with Python
# 3. When satisfied, update binaries:
git add firmware/compiled_binaries/
git commit -m "Update firmware: [feature description]"
```

### Role 2: Python Developer (Laptop B - VSCode)
- Develops data processing, API clients, integration scripts
- Uses pre-compiled binary from repo (no Arduino setup needed)
- Rapid testing without compilation delays

```bash
# Rapid cycle:
# 1. Pull latest binaries from repo
git pull

# 2. Flash once
esptool write-flash 0x0 firmware/compiled_binaries/main_firmware/main.ino.merged.bin

# 3. Iterate on Python code (50+ tests, each 30 sec)
# - person_detector.py
# - camera_client.py
# - data_logger.py
# No hardware access to Arduino needed

# 4. If firmware enhancement needed, request from firmware dev
git issue "Please add /sensor_raw endpoint for raw data"
```

---

## 📈 Time Savings

### Before (Old Workflow - Everyone Compiles)
```
Python test 1:   Compile (15 min) + Upload (30 sec) = 15.5 min
Python test 2:   Compile (15 min) + Upload (30 sec) = 15.5 min
Python test 3:   Compile (15 min) + Upload (30 sec) = 15.5 min
...50 tests:     15.5 min × 50 = 775 minutes (12+ hours!)
```

### After (New Workflow - Binary Caching)
```
Firmware change: Compile (15 min) + Upload (30 sec) = 15.5 min
Upload binaries: git push (1 min) = 1 min
Python test 1-50: Upload (30 sec) × 1 + Test iterations = ~30 minutes
...Total: ~47 minutes for 50 iterations
```

**🎉 Speedup: 16x faster development (12 hours → 45 minutes)**

---

## 🛠️ Quick Reference Commands

### Upload Pre-Compiled Binary
```bash
~/.arduino15/packages/esp32/tools/esptool_py/5.2.0/esptool \
  --port /dev/ttyUSB0 --baud 460800 write-flash 0x0 \
  firmware/compiled_binaries/main_firmware/main.ino.merged.bin
```

### Full Recompile After Firmware Change
```bash
rm -rf ~/.cache/arduino/sketches/*
FQBN="esp32:esp32:esp32s3:PartitionScheme=huge_app"
arduino-cli compile --fqbn $FQBN firmware/main/main.ino && \
  SKETCH_ID=$(ls -t ~/.cache/arduino/sketches/ | head -1) && \
  cp ~/.cache/arduino/sketches/$SKETCH_ID/main.ino.* \
     firmware/compiled_binaries/main_firmware/
```

### Quick Incremental Recompile (Config Changes)
```bash
FQBN="esp32:esp32:esp32s3:PartitionScheme=huge_app"
arduino-cli compile --fqbn $FQBN firmware/main/main.ino
```

### Monitor Serial Output
```bash
python3 -m serial.tools.miniterm /dev/ttyUSB0 460800
# Or with manual reset control:
python3 -m serial.tools.miniterm /dev/ttyUSB0 460800 --dtr=0 --rts=0
```

---

## ✅ Checklist Before Sharing Binaries

When you commit compiled binaries:

- [ ] Firmware compiles without errors
- [ ] Upload succeeds (0x0 address, all bytes verified)
- [ ] Device boots and produces serial output ([BOOT:0] visible)
- [ ] WiFi connects within 10 seconds ([WIFI:5] printed)
- [ ] HTTP API endpoints respond (/health, /image.jpg, etc.)
- [ ] Update firmware/compiled_binaries/README.md with timestamp
- [ ] Commit message includes feature list or fixes

```bash
git add firmware/compiled_binaries/
git commit -m "Firmware v2.3: Add HLK-LD2451 radar integration, fix serial silence bug"
```

---

## 🚨 Debugging When It Breaks

### Upload Hangs at 50%
→ Kill esptool, clear cache, recompile from scratch

### Serial Output is Silent
→ Check firmware has `Serial.begin(460800)` + 3000ms delay in setup()

### Python Client Gets HTTP 503
→ Firmware crashed during setup() - check serial output for errors

### Sensors Not Triggering
→ Verify GPIO pins match firmware (GROVE_PIR_PIN=21, HLK_RX=14, etc.)

---

## 🔗 Related Documentation

- [Pre-Compiled Binaries Guide](firmware/compiled_binaries/README.md)
- [Firmware Source](firmware/main/main.ino)
- [Python Clients](python/)
- [Hardware Pinout](docs/hardware_pinout.txt)
