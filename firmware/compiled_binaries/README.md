# Pre-Compiled Firmware Binaries

This directory contains **pre-compiled firmware binaries** for the ESP32-S3 person detection project. These binaries eliminate the need for long compilation cycles during development and testing.

## ⏱️ Why Pre-Compiled Binaries Matter

- **Clean cache compilation**: ~10-15 minutes
- **Incremental recompilation**: ~1-2 minutes
- **Direct binary upload**: ~30 seconds
- **Saves 90% of development time** when only testing without code changes

## 📦 Contents

### `main_firmware/`
Pre-compiled binaries for the **robust main firmware** (`firmware/main/main.ino`)

Files included:
- `main.ino.bin` - Application binary (1.7 MB)
- `main.ino.bootloader.bin` - Bootloader (20 KB)
- `main.ino.partitions.bin` - Partition table (3 KB)
- `main.ino.merged.bin` - Complete flash image (4.0 MB)
- `main.ino.elf` - Symbol table & debug info (26 MB)
- `main.ino.map` - Memory map (20 MB)

### `reference_firmware/`
Pre-compiled binaries for the **reference implementation** (`archive/firmware/men_ai_model/men_ai_model.ino`)

Same structure as above, named `men_ai_model.ino.*`

## 🚀 Quick Upload (Skip Compilation)

### Option 1: Using esptool directly (Fastest)
```bash
# Upload main firmware
esptool.py -p /dev/ttyUSB0 write_flash \
  0x0 firmware/compiled_binaries/main_firmware/main.ino.bootloader.bin \
  0x8000 firmware/compiled_binaries/main_firmware/main.ino.partitions.bin \
  0x10000 firmware/compiled_binaries/main_firmware/main.ino.bin

# Upload reference firmware
esptool.py -p /dev/ttyUSB0 write_flash \
  0x0 firmware/compiled_binaries/reference_firmware/men_ai_model.ino.bootloader.bin \
  0x8000 firmware/compiled_binaries/reference_firmware/men_ai_model.ino.partitions.bin \
  0x10000 firmware/compiled_binaries/reference_firmware/men_ai_model.ino.bin
```

### Option 2: Using merged binary (Even simpler)
```bash
# Main firmware
esptool.py -p /dev/ttyUSB0 write_flash 0x0 \
  firmware/compiled_binaries/main_firmware/main.ino.merged.bin

# Reference firmware
esptool.py -p /dev/ttyUSB0 write_flash 0x0 \
  firmware/compiled_binaries/reference_firmware/men_ai_model.ino.merged.bin
```

### Option 3: Using Arduino CLI (Hybrid approach)
Create a custom upload script using Arduino CLI with pre-compiled binaries:
```bash
FQBN="esp32:esp32:esp32s3:PartitionScheme=huge_app"
arduino-cli upload -p /dev/ttyUSB0 --fqbn $FQBN \
  --input-dir firmware/compiled_binaries/main_firmware \
  firmware/main/main.ino
```

## 📝 Hardware Configuration

These binaries are compiled for:
- **Device**: ESP32-S3 CAM (GOOUUU variant)
- **Partition Scheme**: `huge_app` (3.15 MB app space)
- **Baud Rate**: 460800
- **FQBN**: `esp32:esp32:esp32s3:PartitionScheme=huge_app`

## 🔄 Updating Binaries

When you **modify the source code** and recompile, update these binaries:

```bash
# After successful compilation
SKETCH_ID="0843F89DB9E2F373B881761D63A4CE30"  # main.ino ID
CACHE_DIR="~/.cache/arduino/sketches/$SKETCH_ID"

cp $CACHE_DIR/main.ino.* firmware/compiled_binaries/main_firmware/
git add firmware/compiled_binaries/
git commit -m "Update pre-compiled main firmware after code changes"
```

## 📊 Compilation Timestamps

| Firmware | Compiled | Size | Status |
|----------|----------|------|--------|
| main.ino | 2026-06-18 18:34 | 1.7M | ✅ Ready |
| men_ai_model.ino | 2026-06-18 18:29 | 1.7M | ✅ Ready |

## 🐛 Troubleshooting

### ⚠️ Current Issue: Upload Hangs or Device Silent After Boot

**Symptoms:**
- esptool upload stops mid-way (e.g., at 50-70% progress)
- Device boots but produces ZERO serial output
- Both main and reference firmware exhibit identical silent behavior
- Chip responds to esptool commands but application firmware doesn't print

**Root Cause Analysis:**
The pre-compiled binaries may have been created with a firmware bug or incompatibility. Since BOTH firmware versions are silent on serial, this suggests:

1. **Firmware Logic Issue** - Critical bug during initialization causing hang/crash before first serial.println()
2. **Compilation Cache Corruption** - Stale cache causing binaries to include bad object files
3. **UART Configuration Mismatch** - Serial baud rate (460800) not matching hardware expectations

**Immediate Fix (Recommended):**

```bash
# Step 1: Clean everything
rm -rf ~/.cache/arduino/*
rm -rf ~/.arduino15/packages/esp32/hardware/esp32/3.3.8/cores/*

# Step 2: Force full recompile from source
FQBN="esp32:esp32:esp32s3:PartitionScheme=huge_app"
arduino-cli compile --fqbn $FQBN firmware/main/main.ino 2>&1 | tee recompile.log

# Step 3: Extract fresh binaries
SKETCH_ID=$(grep -o "sketches/[A-F0-9]*" recompile.log | cut -d/ -f2 | head -1)
cp ~/.cache/arduino/sketches/$SKETCH_ID/main.ino.* \
   firmware/compiled_binaries/main_firmware/

# Step 4: Test upload
~/.arduino15/packages/esp32/tools/esptool_py/5.2.0/esptool \
  --port /dev/ttyUSB0 --baud 460800 write-flash 0x0 \
  firmware/compiled_binaries/main_firmware/main.ino.merged.bin

# Step 5: Monitor output
python3 -m serial.tools.miniterm /dev/ttyUSB0 460800
```

### Binary mismatch error
If you get "Incorrect length" or similar errors:
1. Clear Arduino cache: `rm -rf ~/.cache/arduino/`
2. Recompile from source
3. Update binaries in this directory

### Upload fails with "Invalid address"
Ensure partition scheme matches:
```bash
~/.arduino15/packages/esp32/tools/esptool_py/5.2.0/esptool \
  -p /dev/ttyUSB0 read_partition_table
```

Should show:
- Bootloader: 0x0 (20KB)
- Partitions: 0x8000 (4KB)
- App: 0x10000 (3.15MB)

### Serial output not visible
If uploaded binary boots but serial is silent:
1. Check baud rate is 460800 in firmware (line ~640 in main.ino)
2. Verify USB cable quality and connection
3. Try reference firmware: `esptool write-flash 0x0 reference_firmware/men_ai_model.ino.merged.bin`
4. Check for initialization bugs in setup() - device may be crashing before first print
5. If BOTH firmwares silent → Likely hardware/USB driver issue, not code

## 🛠️ Development Workflow

### 🚀 Efficient Development (Minimize Compile-Test Cycles)

**Philosophy:** Separate firmware changes from Python client changes. Recompile only when needed.

#### Workflow A: Python Client Development (No Firmware Changes)
```bash
# FASTEST: Use pre-compiled binary (30 seconds per test)
~/.arduino15/packages/esp32/tools/esptool_py/5.2.0/esptool \
  --port /dev/ttyUSB0 --baud 460800 write-flash \
  0x0 firmware/compiled_binaries/main_firmware/main.ino.bin \
  0x8000 firmware/compiled_binaries/main_firmware/main.ino.partitions.bin \
  0x0 firmware/compiled_binaries/main_firmware/main.ino.bootloader.bin

# Wait for device to boot (3-5 seconds)
sleep 5

# Run your Python client
python3 python/main_client.py  # or person_detector.py, etc.
```

**Time Saved:** ~14.5 minutes per test (full compile = 15 min, esptool = 30 sec)

#### Workflow B: Firmware Code Changes (Full Recompile Needed)
```bash
# 1. Make code changes
nano firmware/main/main.ino

# 2. Clean cache to force full rebuild (takes 15-20 minutes)
rm -rf ~/.cache/arduino/sketches/*

# 3. Compile
FQBN="esp32:esp32:esp32s3:PartitionScheme=huge_app"
arduino-cli compile --fqbn $FQBN firmware/main/main.ino 2>&1 | tee compile.log

# 4. Find compiled binaries in cache
SKETCH_ID=$(grep -o "sketches/[A-F0-9]*" compile.log | cut -d/ -f2 | head -1)
CACHE_DIR="~/.cache/arduino/sketches/$SKETCH_ID"

# 5. Update repository binaries
cp $CACHE_DIR/main.ino.* firmware/compiled_binaries/main_firmware/

# 6. Quick upload & test
~/.arduino15/packages/esp32/tools/esptool_py/5.2.0/esptool \
  --port /dev/ttyUSB0 --baud 460800 write-flash 0x0 \
  firmware/compiled_binaries/main_firmware/main.ino.merged.bin
```

#### Workflow C: Configuration Changes Only (No Recompile)
```bash
# Edit just the constants (WiFi, pins, timeouts, etc.)
# Lines 20-40 in main.ino - these compile instantly

nano firmware/main/main.ino  # Change WIFI_SSID, GPIO pins, timeouts, etc.

# Partial recompile (1-2 minutes instead of 15)
FQBN="esp32:esp32:esp32s3:PartitionScheme=huge_app"
arduino-cli compile --fqbn $FQBN firmware/main/main.ino

# Upload new config
~/.arduino15/packages/esp32/tools/esptool_py/5.2.0/esptool \
  --port /dev/ttyUSB0 --baud 460800 write-flash 0x0 \
  firmware/compiled_binaries/main_firmware/main.ino.merged.bin
```

### 📊 Time Comparison

| Task | Time | When to Use |
|------|------|-----------|
| Full firmware recompile | 15-20 min | Logic changes, new features, sensor code |
| Config-only change | 1-2 min | WiFi SSID, GPIO pins, timeouts |
| Binary upload (esptool) | 30 sec | Any test, any client script |
| Arduino IDE full cycle | 20+ min | Avoid - use esptool for uploads |

## 📌 Important Notes

- **Do NOT commit these large files to git** - Consider using Git LFS if needed
- **Binaries are device-specific** - Only work with ESP32-S3 with 8MB PSRAM, 16MB Flash
- **Verify checksum** after upload - esptool does this automatically
- **Keep source code in sync** - Document any manual binary changes

## 🔗 Related Files

- Source: [firmware/main/main.ino](../main/main.ino)
- Reference: [archive/firmware/men_ai_model/men_ai_model.ino](../../archive/firmware/men_ai_model/men_ai_model.ino)
- Troubleshooting: [docs/troubleshooting/ROOT_CAUSE_ANALYSIS.md](../../docs/troubleshooting/ROOT_CAUSE_ANALYSIS.md)
- Build info: [docs/guides/QUICK_START.md](../../docs/guides/QUICK_START.md)
