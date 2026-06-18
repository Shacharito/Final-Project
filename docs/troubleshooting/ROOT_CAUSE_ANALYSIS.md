# ESP32-S3 Boot Crash - ROOT CAUSE & RECOVERY

## ⚠️ CRITICAL FINDING: WebServer Initialization Order

### The Problem
ESP32 WebServer library **REQUIRES WiFi to be fully initialized first** before calling `server.begin()`. 

**Error when done wrong:**
```
assert failed: xQueueSemaphoreTake queue.c:1709 (( pxQueue ))
```

This is a FreeRTOS kernel panic due to WebServer trying to use WiFi's event queue before it exists.

---

## Current Device State
- **Status**: Stuck in reboot loop (from failed combined test upload)
- **Action Required**: Power cycle device (disconnect/reconnect USB)
- **Expected After Reboot**: Device should stabilize, but needs firmware upload

---

## Recovery Steps

### Step 1: Power Cycle Device
1. Disconnect USB cable from ESP32 (or computer)
2. Wait 3 seconds
3. Reconnect USB cable
4. Wait for device to fully boot (~3 seconds)

### Step 2: Verify Main Firmware Status
The main/main.ino has CORRECT initialization order:
```cpp
// Line 338-341: WiFi first
WiFi.begin(wifi_ssid, wifi_password);
...
// Line 365: WebServer AFTER WiFi connects
server.begin();
```

✅ **No changes needed to main firmware**

### Step 3: Flash Corrected Firmware

Main firmware already has correct order. Upload it:
```bash
FQBN="esp32:esp32:esp32s3:PartitionScheme=huge_app"
arduino-cli compile --fqbn $FQBN firmware/main/main.ino
arduino-cli upload -p /dev/ttyUSB0 --fqbn $FQBN firmware/main/main.ino
```

### Step 4: Verify Boot Success
```bash
python3 << 'EOF'
import serial, time
ser = serial.Serial('/dev/ttyUSB0', 460800, timeout=2)
time.sleep(1)
data = ser.read(2000).decode('utf-8', errors='ignore')
print(data[:400])

if "WiFi connected" in data and "Web server started" in data:
    print("\n✅ SUCCESS - Firmware booted correctly!")
elif "panic" in data or "assert failed" in data:
    print("\n❌ FAILED - Still crashing")
else:
    print("\n⚠️  Unclear - check output above")
ser.close()
EOF
```

---

## What Was Wrong

### Diagnostic Firmware (INCORRECT - DON'T USE)
```cpp
// Line 13: Creates WebServer TOO EARLY
WebServer server(80);

void setup() {
  // ...
  // Line 28: Starts server BEFORE WiFi
  server.begin();
  
  // Line 33: WiFi initialized AFTER server.begin()
  WiFi.begin(ssid, password);  // ❌ TOO LATE!
}
```

### Main Firmware (CORRECT - ALREADY FIXED)
```cpp
void setup() {
  // Step 1: Initialize WiFi
  WiFi.begin(wifi_ssid, wifi_password);
  while (WiFi.status() != WL_CONNECTED && attempts < 20) {
    delay(500);
  }
  
  // Step 2: Only AFTER WiFi connected
  server.on("/run", HTTP_GET, handleRunInference);
  server.begin();  // ✅ CORRECT - Called after WiFi ready
}
```

---

## Summary

| Component | Status | Issue |
|-----------|--------|-------|
| **Hardware** | ✅ Working | No issues found |
| **Sensors/Camera** | ✅ Not the problem | Factory test proves device runs fine |
| **WiFi Alone** | ✅ Works | Device connects successfully |
| **WebServer Alone** | ❌ Crashes | Needs WiFi first |
| **WiFi + WebServer** | ✅ Should work | If WiFi init comes first |
| **Main Firmware** | ✅ Correct | Already has proper order |

---

## Next Action
1. **Physically power-cycle the device** (disconnect/reconnect USB)
2. **Wait 3 seconds** for device to stabilize
3. **Upload main firmware** using correct FQBN
4. **Verify** using Python serial test above

Device should then:
- Boot successfully
- Connect to WiFi
- Accept HTTP requests on port 80
- Run person detection inference
- Respond to client commands

