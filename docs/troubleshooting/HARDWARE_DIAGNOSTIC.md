# Hardware Diagnostic Checklist

## Current Issues
- Device not reachable at 172.20.10.2
- Upload failing with "exit status 2"
- Serial output corrupted
- **Missing: Sensors may not be connected**

---

## STEP 1: Physical Hardware Inspection

### ESP32-S3-CAM Module
- [ ] USB cable connected to VM host?
- [ ] Power LED on ESP32 lit?
- [ ] Camera module **securely seated** in connector?
- [ ] No bent pins on camera connector?

### Camera Module (OV3660)
- [ ] Flex cable **fully inserted** into camera socket?
- [ ] Flex cable not kinked or folded?
- [ ] Correct orientation (gold contacts facing inward)?

### Sensors (If connected)
- [ ] All sensor wires connected?
- [ ] GPIO connections match `config.h`?
- [ ] No loose wires?

---

## STEP 2: USB Connection Check (On Linux/VM)

```bash
# 1. Check if USB device is detected by OS
lsusb | grep -i espressif

# 2. Check if serial device appears
ls -la /dev/ttyUSB*

# 3. Check dmesg for USB events
dmesg | tail -20 | grep -i usb

# 4. Check if device is in bootloader mode
dmesg | tail -20 | grep -i "ftdi\|ch340\|cp210x"
```

**Expected output:**
- Should see device like: `Bus 001 Device 004: ID 303a:1001 Espressif ...`
- Should see: `/dev/ttyUSB0` (character device, 0666 permissions)

---

## STEP 3: Power & Reset Verification

```bash
# Reset device and observe
# - Device should reboot
# - Should see "ets_main.c:0" messages on serial
# - Then "ota_begin" messages

stty -F /dev/ttyUSB0 115200 raw
timeout 3 cat /dev/ttyUSB0 | head -20
```

**Expected output:** Bootloader messages, not corrupted data

---

## STEP 4: Test Arduino-CLI Detection

```bash
# List connected boards
arduino-cli board list

# Expected: Should show "/dev/ttyUSB0" as "ESP32-S3 (FQBN: esp32:esp32:esp32s3)"
```

If device not found:
- Device may be in bootloader (normal)
- May need `--discovery` flag
- USB driver issue

---

## STEP 5: Upload with Verbose Output

```bash
# Compile only (no upload)
arduino-cli compile -v --fqbn esp32:esp32:esp32s3:PartitionScheme=huge_app firmware/main/main.ino

# If compile succeeds, then upload
arduino-cli upload -v -p /dev/ttyUSB0 --fqbn esp32:esp32:esp32s3:PartitionScheme=huge_app firmware/main/main.ino 2>&1 | tee /tmp/upload.log

# Check for specific errors
grep -i "error\|failed\|timeout" /tmp/upload.log
```

---

## STEP 6: Serial Monitor After Successful Upload

```bash
# Set baud and read for 10 seconds
stty -F /dev/ttyUSB0 460800 raw
timeout 10 cat /dev/ttyUSB0

# Should see:
# === Main Application Start ===
# Boot count: X
# Wakeup reason: Power on or reset
# Connecting to Amit_wifi ...
# WiFi connected. IP: 172.20.10.2
```

---

## STEP 7: Network Connectivity Test

Once device boots and connects to WiFi:

```bash
# Ping device
ping -c 5 172.20.10.2

# HTTP GET to check web server
curl -v http://172.20.10.2/get_config

# Should return JSON config
```

---

## Troubleshooting by Symptom

### If: USB device not detected
- **Check:** Is VM bridged or NAT? Should be bridged for USB passthrough
- **Fix:** Shutdown VM, restart, check VirtualBox USB settings

### If: Serial output is corrupted/garbage
- **Check:** Baud rate (should be 460800, NOT 115200)
- **Check:** USB cable quality (sometimes bad cables = garbage)
- **Fix:** Try different USB port, different USB cable

### If: Upload fails with "exit status 2"
- **Check:** Device in bootloader? (Normal, should auto-recover)
- **Check:** Is previous upload still in progress?
- **Fix:** Try manual board reset: Press RST button on ESP32, then upload immediately

### If: Device boots but WiFi doesn't connect
- **Check:** WiFi credentials correct in `config.h`?
- **Check:** WiFi network is 2.4GHz (not 5GHz)?
- **Check:** Is "Amit_wifi" powered on?
- **Fix:** Change credentials via `/set_config` endpoint

### If: Device boots but doesn't get IP address
- **Check:** DHCP working on your WiFi router?
- **Check:** Is ESP32 in WiFi range?
- **Fix:** Check router logs, ensure DHCP enabled

---

## What to Report After Diagnostics

When confirming status, provide:
```
✓ USB device detected: YES/NO
✓ Serial communication: WORKING/CORRUPTED/NONE
✓ Device boots: YES/NO  
✓ WiFi connects: YES/NO
✓ Device gets IP: YES/NO
✓ HTTP responds: YES/NO
✓ Inference endpoint works: YES/NO
```

