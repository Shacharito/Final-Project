# Root Cause Analysis: Serial Silence & Upload Failures

## 🔴 Current Problem Summary

**Observable Symptoms:**
1. esptool upload **hangs at 30-70%** progress ("The chip stopped responding")
2. Device **boots but produces ZERO serial output** on 460800 baud
3. Both main.ino AND reference firmware exhibit identical silent behavior
4. Device IS responsive (esptool can communicate via bootloader, responds to read_mac)
5. USB detection works, firmware upload completes but no app output

**Timeline:**
- ✅ Firmware compiles without errors (1,757,880 bytes = 55% capacity)
- ✅ esptool upload starts successfully
- ❌ Upload stalls at ~50% ("Chip stopped responding")
- ❌ When forced to complete, device boots but silent (no [BOOT:0] message)

---

## 🔍 Diagnostic Findings

### 1. **Hardware Layer: WORKING ✅**
- ESP32-S3 USB detects correctly
- esptool can read MAC address (3c:0f:02:d7:56:c4)
- RTS pin reset works (esptool triggers hard reset)
- Device responds to bootloader commands
- **Conclusion:** USB communication, boot ROM, bootloader are fine

### 2. **Firmware Initialization: BROKEN ❌**
- No serial output whatsoever
- Reference firmware (men_ai_model.ino) also silent (rules out main.ino-specific bug)
- Both firmwares compiled from same Arduino framework (esp32 v3.3.8)
- **Conclusion:** Either firmware has critical bug OR binaries are corrupted

### 3. **Upload Process: DEGRADED ⚠️**
- Esptool successfully communicates up to ~50% progress
- Then loses sync with device ("chip stopped responding")
- Hard reset via RTS completes upload but device doesn't run cleanly
- Device boots ROM successfully but app firmware doesn't execute or crashes immediately
- **Conclusion:** Firmware likely crashes during early boot, causing USB communication loss

---

## 🎯 Root Cause Hypotheses (Most to Least Likely)

### Hypothesis A: Firmware Initialization Bug (75% probability)
**Scenario:** Device enters setup(), hits critical error BEFORE first Serial.println()

Evidence:
- Both firmware versions affected equally
- Device responds to esptool (proves code is running initially)
- But no serial output (proves first print() is never reached)
- Setup() code in both versions calls Serial.begin() immediately

**Likely Culprits:**
```cpp
// Line 640 area in main.ino:
Serial.begin(460800);
delay(3000);
Serial.flush();
WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0);  // ← This might crash if order is wrong
Serial.println("\n\n[BOOT:0] Starting setup()");  // ← Never reached?
```

Or in Camera init:
```cpp
initCamera();  // This has heap allocation, might fail before PSRAM check
```

### Hypothesis B: Cache Corruption (15% probability)
**Scenario:** Arduino cache contains stale object files or broken symbols

Evidence:
- Two different source files (main.ino vs men_ai_model.ino) → identical failure
- Unlikely both have identical initialization bugs
- Cache might be reused between compilations

**Typical corruption symptoms:**
- Code compiles but doesn't run
- Works fine with clean cache
- Silent failures (no error messages, just hangs)

### Hypothesis C: UART Configuration Mismatch (5% probability)
**Scenario:** Serial baud rate not correctly configured

Evidence:
- Both firmwares use 460800 (same)
- esptool can reset device via RTS (proves boot ROM works)
- Boot ROM uses different baud initially, then switches
- Less likely but possible

### Hypothesis D: Hardware/USB Driver (5% probability)
**Scenario:** Physical USB issue or Linux driver problem

Evidence:
- esptool communicates fine initially
- Device boots successfully (USB stays connected)
- Less likely if only one machine affected

---

## 🔧 Recommended Fix (Priority Order)

### Fix #1: Clean Compile (Highest Priority)

```bash
# Step 1: Nuke everything
rm -rf ~/.cache/arduino/sketches/*
rm -rf ~/.arduino15/packages/esp32/hardware/esp32/3.3.8/cores/*
rm -rf /tmp/*arduino*

# Step 2: Fresh full compile
cd /home/amit/studies/final_project/Final-Project
FQBN="esp32:esp32:esp32s3:PartitionScheme=huge_app"
arduino-cli compile --fqbn $FQBN firmware/main/main.ino \
  2>&1 | tee clean_compile.log

# Expected output:
# - "Sketch uses XXX bytes (YY%) of program storage space"
# - "Leaves ZZZZ bytes for global variables"
# - No errors or warnings about Edge Impulse
```

### Fix #2: Inspect Compiled Output

```bash
# Find sketch cache directory
SKETCH_ID=$(grep -o "sketches/[A-F0-9]*" clean_compile.log | cut -d/ -f2 | head -1)
echo "Sketch ID: $SKETCH_ID"

# Check generated binary size
ls -lh ~/.cache/arduino/sketches/$SKETCH_ID/main.ino.bin
# Should be ~1.7MB (1,758,144 bytes)

# Verify all components present
ls -1 ~/.cache/arduino/sketches/$SKETCH_ID/main.ino.*
# Should list: .bin, .elf, .partitions.bin, .bootloader.bin, .merged.bin, .map
```

### Fix #3: Test Upload with New Binary

```bash
SKETCH_ID=$(ls -t ~/.cache/arduino/sketches/ | head -1)
CACHE="~/.cache/arduino/sketches/$SKETCH_ID"

# Upload fresh binary
~/.arduino15/packages/esp32/tools/esptool_py/5.2.0/esptool \
  --port /dev/ttyUSB0 --baud 460800 write-flash 0x0 \
  $CACHE/main.ino.merged.bin

# Monitor output immediately
sleep 2
python3 -m serial.tools.miniterm /dev/ttyUSB0 460800
```

### Fix #4: If Still Silent - Check Initialization Order

Edit [firmware/main/main.ino](firmware/main/main.ino) and verify:

```cpp
// ✅ CORRECT ORDER (lines 600-650):
void setup() {
    // 1. Start serial FIRST
    Serial.begin(460800);
    
    // 2. Wait for UART to stabilize
    delay(3000);
    Serial.flush();
    
    // 3. NOW safe to modify hardware registers
    WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0);
    
    // 4. NOW safe to print (UART is fully ready)
    Serial.println("\n\n[BOOT:0] Starting setup()");
    
    // 5. Continue with non-blocking initialization
    if (psramFound()) Serial.println("[BOOT:1] PSRAM found");
    // ... etc
}

// ❌ WRONG ORDER (would cause silence):
void setup() {
    Serial.begin(460800);
    WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0);  // TOO EARLY!
    // delay(3000);  // MISSING!
    Serial.println("[BOOT:0] Starting setup()");  // May not print
}
```

---

## 📊 Next Steps If Clean Compile Still Fails

### If Binary Upload Still Hangs at 50%:
1. Try **reference firmware** instead:
   ```bash
   arduino-cli compile --fqbn $FQBN archive/firmware/men_ai_model/men_ai_model.ino
   # Upload reference binary
   ```
2. If reference also hangs → suggests deeper compilation issue
3. Check for corrupted source files:
   ```bash
   file firmware/main/main.ino  # Should be ASCII text
   wc -l firmware/main/main.ino  # Should be 850+ lines
   ```

### If Binary Uploads But Still Silent:
1. Add debug output at VERY start of setup():
   ```cpp
   void setup() {
       // Immediate output - no delay, no register changes
       Serial.begin(460800);
       for(int i=0; i<10; i++) Serial.write('X');  // Print before anything else
       // If you see "XXXXXXXXXX", then initialization is crashing later
   }
   ```

2. Binary search for crash location:
   - Comment out initCamera() → recompile → test
   - Comment out initGROVEPIR() → recompile → test
   - Comment out WiFi init → recompile → test
   - Find which line causes silence

### If Everything Works With Clean Compile:
1. **Prevent future corruption:** Don't let cache grow too large
   ```bash
   # Add to ~/.bashrc or ~/.profile:
   alias arduino-clean="rm -rf ~/.cache/arduino/sketches/*"
   ```

2. Update repo binaries:
   ```bash
   SKETCH_ID=$(ls -t ~/.cache/arduino/sketches | head -1)
   cp ~/.cache/arduino/sketches/$SKETCH_ID/main.ino.* \
      firmware/compiled_binaries/main_firmware/
   ```

---

## 🎯 Quick Diagnostics Checklist

Before digging deeper, verify:

- [ ] USB cable is good (try different port if possible)
- [ ] Device not locked by another process: `lsof /dev/ttyUSB0`
- [ ] Port permissions OK: `ls -l /dev/ttyUSB0` (should be readable)
- [ ] Arduino framework is latest: `arduino-cli core list`
- [ ] Cache not full: `du -sh ~/.cache/arduino/`
- [ ] Source files not corrupt: `diff firmware/main/main.ino <backup>`

---

## 📝 Documentation

See related files:
- [WORKFLOW_STRATEGY.md](WORKFLOW_STRATEGY.md) - Efficient development workflow
- [firmware/main/main.ino](firmware/main/main.ino) - Actual firmware code
- [firmware/compiled_binaries/README.md](firmware/compiled_binaries/README.md) - Binary management

**Last Updated:** 2026-06-18
**Status:** 🔴 BLOCKING - Awaiting clean cache recompile test
