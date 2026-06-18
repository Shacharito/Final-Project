# Robust Firmware Architecture - Complete Guide

## Overview
The updated main firmware is now **fault-tolerant** and can operate even with missing or failing sensors. Device will:
- ✅ Boot successfully even if camera is disconnected
- ✅ Continue running all other functions
- ✅ Provide diagnostic endpoints to check status
- ✅ Handle errors gracefully without crashing
- ✅ Clear logging at each step for debugging

---

## Key Architecture Sections

### SECTION 1: Configuration & State
- **Device State Structure** tracks what's working:
  ```cpp
  struct DeviceState {
    bool wifi_connected;           // WiFi status
    bool camera_initialized;       // Camera ready?
    bool model_loaded;             // ML model ready?
    uint32_t camera_init_error;    // Error code if camera failed
    uint32_t last_inference_ms;    // Performance metric
    uint32_t last_capture_error;   // Capture error tracking
    unsigned long uptime_seconds;  // Device uptime
  }
  ```
- Enables graceful degradation - device knows what's available

### SECTION 2: Utility Functions
**Robust error handling:**
- `ei_malloc()` - Logs allocation failures
- `getErrorMessage()` - Human-readable error codes  
- `getUptimeSeconds()` - System health metric
- `get_data()` - Safe callback with NULL checks

### SECTION 3: Power Management
- Clean shutdown sequence
- Proper cleanup of buffers before sleep
- Clear logging of sleep state

### SECTION 4: Hardware Initialization (FAULT TOLERANT ⭐)
**Critical change: Camera failure is NO LONGER FATAL**

```cpp
bool initCamera() {
    // ... setup code ...
    
    esp_err_t err = esp_camera_init(&config);
    if (err != ESP_OK) {
        device_state.camera_init_error = err;  // Log error
        Serial.printf("[CAMERA] ❌ FAILED: 0x%x\n", err);
        Serial.println("[CAMERA] Continuing without camera...");
        device_state.camera_initialized = false;
        return false;  // Don't crash!
    }
    
    device_state.camera_initialized = true;
    return true;
}
```

**Before:**  Camera init fails → Crash/Sleep  
**After:**  Camera init fails → Continue running, report error

### SECTION 5: Web Server Endpoints (FAULT TOLERANT ⭐)

#### Core Endpoints
| Endpoint | Status | Behavior |
|----------|--------|----------|
| `GET /run` | ⭐ Robust | Returns error if camera missing, doesn't crash |
| `GET /image.jpg` | ⭐ Robust | Returns 503 if camera not ready |
| `POST /set_config` | ✅ Always | Update settings anytime |
| `GET /get_config` | ✅ Always | Get current settings |
| `POST /sleep` | ✅ Always | Request deep sleep |

#### New Diagnostic Endpoints (Always Work!)
**`GET /health`** - Quick system health check
```json
{
  "ok": true,
  "wifi_connected": true,
  "camera_ready": false,
  "uptime_sec": 245,
  "heap_free": 1234567,
  "boot_count": 42
}
```

**`GET /diagnose`** - Full diagnostic report
```json
{
  "device_info": {
    "mac_address": "3C:0F:02:D7:56:C4",
    "local_ip": "172.20.10.2",
    "signal_strength": -45,
    "uptime_sec": 245,
    "boot_count": 42
  },
  "hardware": {
    "camera_initialized": false,
    "camera_init_error": "0x20004",
    "last_capture_error": 0,
    "last_inference_ms": 0
  },
  "memory": {
    "heap_free": 1234567,
    "heap_total": 4000000,
    "psram_free": 8000000,
    "psram_total": 8000000
  }
}
```

### SECTION 6: Initialization & Runtime

**Robust Boot Sequence:**
```
Boot ↓
├─ Display startup banner
├─ WiFi Connection (REQUIRED - sleep if fails)
│
├─ Hardware Init (NON-FATAL)
│  ├─ Try camera init
│  ├─ If fails: Log error, mark unavailable, CONTINUE
│  └─ Never crash or sleep
│
├─ WebServer Setup (Always works)
│  ├─ Register all endpoints
│  ├─ Register diagnostic endpoints
│  └─ Start server
│
└─ Ready! Display endpoint list
```

---

## Error Handling Strategy

### Levels of Robustness

**Level 1: CRITICAL** (Device sleeps if fails)
- WiFi connection
- Reason: No remote connectivity without it

**Level 2: IMPORTANT** (Device reports but continues)
- Camera initialization
- Image capture
- Inference execution
- Reason: Can diagnose issues via /health and /diagnose endpoints

**Level 3: ADVISORY** (Logged but doesn't affect operation)
- Memory allocation failures
- Individual sensor errors
- Reason: Device can still operate in degraded mode

---

## Testing Scenarios

### Scenario 1: Camera Not Connected
```
[BOOT] Camera init FAILED: 0x20004
[BOOT] Device will continue without inference
[API] GET /run → HTTP 503 "Camera not initialized"
[API] GET /health → Shows camera_ready: false
[API] Other endpoints → Work normally
```

### Scenario 2: WiFi Connection Fails
```
[INIT] WiFi connection FAILED
[BOOT] Entering deep sleep for 60 seconds
[BOOT] Device will retry on next wakeup
```

### Scenario 3: Memory Allocation Fails
```
[CAPTURE] Memory allocation failed
[API] GET /run → HTTP 500 "Capture failed"
[API] Device stays running, next attempt may succeed
```

---

## Usage Examples

### Quick Health Check
```bash
curl http://172.20.10.2/health
# Returns immediately with system status
```

### Diagnose Missing Camera
```bash
curl http://172.20.10.2/diagnose | jq '.hardware'
# Shows: camera_initialized: false, camera_init_error: 0x20004
```

### Try Inference With Error Handling
```bash
curl http://172.20.10.2/run
# If camera missing: HTTP 503 with error details
# If camera ready: HTTP 200 with inference results
```

---

## Logging Format

Each component has clear prefixes for easy troubleshooting:

```
[BOOT]     - Startup/boot information
[INIT]     - Initialization sequence
[WIFI]     - WiFi operations
[CAMERA]   - Camera operations
[CAPTURE]  - Image capture
[API]      - Web API requests
[CONFIG]   - Configuration changes
[SLEEP]    - Sleep mode operations
[ERROR]    - Error conditions
```

---

## Python Client Adaptation

The client should handle new error codes:

```python
def run_inference(self):
    try:
        response = self.session.get(f'{self.base_url}/run', timeout=5)
        
        if response.status_code == 503:
            # Camera not available
            print("Camera not available - check /diagnose")
            return None
            
        elif response.status_code == 500:
            # Inference failed
            data = response.json()
            print(f"Inference failed: {data.get('error')}")
            return None
            
        elif response.status_code == 200:
            return response.json()
            
    except Exception as e:
        print(f"Connection error: {e}")
        return None
```

---

## Monitoring Tips

1. **First boot:** Check `/diagnose` to see what's connected
2. **Regular health:** Poll `/health` endpoint (lightweight)
3. **Before inference:** Check `camera_ready` field
4. **After failures:** Review error codes in `/diagnose`
5. **Performance:** Track `last_inference_ms` for trends

---

## Benefits of This Architecture

✅ **Robustness:** Fails gracefully, doesn't crash  
✅ **Debuggability:** Clear logging and diagnostic endpoints  
✅ **Flexibility:** Works with partial hardware  
✅ **Safety:** Non-fatal hardware failures don't cause sleep cycles  
✅ **Maintainability:** Clear sections and error codes  
✅ **Performance:** Tracks metrics for optimization  

---

## Next Steps

1. **Power cycle device** (disconnect/reconnect USB)
2. **Upload new firmware**:
   ```bash
   FQBN="esp32:esp32:esp32s3:PartitionScheme=huge_app"
   arduino-cli compile --fqbn $FQBN firmware/main/main.ino
   arduino-cli upload -p /dev/ttyUSB0 --fqbn $FQBN firmware/main/main.ino
   ```

3. **Test without camera:**
   ```bash
   curl http://172.20.10.2/health
   curl http://172.20.10.2/diagnose | jq
   ```

4. **Connect camera later** and it will work automatically!

