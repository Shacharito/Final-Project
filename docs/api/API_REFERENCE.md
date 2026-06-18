# API Quick Reference - Robust Firmware

## Base URL
```
http://172.20.10.2
```

---

## Endpoint Summary

### 🔧 Inference & Imaging
| Method | Endpoint | Purpose | Fails? |
|--------|----------|---------|--------|
| GET | `/run` | Run person detection | 503 if no camera |
| GET | `/image.jpg` | Get last captured image | 404 if none |

### ⚙️ Configuration  
| Method | Endpoint | Purpose | Notes |
|--------|----------|---------|-------|
| GET | `/get_config` | Get device settings | Always works |
| POST | `/set_config` | Update settings | Always works |
| POST | `/sleep` | Request deep sleep | Puts device to sleep |

### 🏥 Diagnostics & Health
| Method | Endpoint | Purpose | Always Works? |
|--------|----------|---------|--------------|
| GET | `/health` | Quick system status | ✅ Yes |
| GET | `/diagnose` | Full diagnostic report | ✅ Yes |

---

## Detailed Endpoint Documentation

### GET `/run` - Run Inference
**Purpose:** Capture image and run person detection  
**Response on success (200):**
```json
{
  "ok": true,
  "top_label": "person",
  "top_score": 0.9234,
  "inference_ms": 234,
  "labels": {
    "person": 0.9234,
    "no_person": 0.0766
  }
}
```

**Responses on failure:**
- `503 Unavailable` - Camera not initialized
- `500 Error` - Capture failed (see error details)

---

### GET `/image.jpg` - Get Last Image
**Purpose:** Download last captured JPEG image  
**Response on success (200):**
- Raw JPEG binary data (image/jpeg)

**Responses on failure:**
- `404 Not Found` - No image captured yet
- `503 Unavailable` - Camera not initialized

---

### GET `/get_config` - Get Configuration
**Purpose:** Retrieve current device settings  
**Response (200):**
```json
{
  "ok": true,
  "ssid": "Amit_wifi",
  "sleep_sec": 60,
  "person_thresh": 0.7,
  "boot_count": 5,
  "wakeup_reason": "Power-on or reset",
  "uptime_sec": 245
}
```

---

### POST `/set_config` - Update Configuration
**Purpose:** Change device settings  
**Query parameters:**
```
ssid=NewSSID
password=NewPassword
sleep_sec=120
person_thresh=0.8
```

**Example:**
```bash
curl -X POST "http://172.20.10.2/set_config?ssid=MyWiFi&sleep_sec=120"
```

**Response (200):**
```json
{
  "ok": true,
  "updated": true
}
```

---

### POST `/sleep` - Request Deep Sleep
**Purpose:** Put device into deep sleep mode  
**Query parameters (optional):**
```
duration=60  # Override default sleep duration in seconds
```

**Example:**
```bash
curl -X POST "http://172.20.10.2/sleep?duration=300"
```

**Response (200, sent before sleep):**
```json
{
  "ok": true,
  "sleeping_for": 300
}
```

---

### GET `/health` - System Health Check ⭐ NEW
**Purpose:** Quick status check (lightweight)  
**Response (200):**
```json
{
  "ok": true,
  "wifi_connected": true,
  "camera_ready": true,
  "uptime_sec": 245,
  "heap_free": 1234567,
  "boot_count": 5
}
```

**Key fields:**
- `wifi_connected`: Always true for this response (means WiFi is working)
- `camera_ready`: `true` if camera is initialized and working
- `uptime_sec`: Device uptime since last boot
- `heap_free`: Available RAM in bytes
- `boot_count`: Total number of boots (persists across reboots)

---

### GET `/diagnose` - Full Diagnostics ⭐ NEW
**Purpose:** Complete device diagnostics and debugging  
**Response (200):**
```json
{
  "device_info": {
    "mac_address": "3C:0F:02:D7:56:C4",
    "local_ip": "172.20.10.2",
    "signal_strength": -45,
    "uptime_sec": 245,
    "boot_count": 5
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

**Interpreting error codes:**
- `camera_init_error: "0x20004"` = Camera XCLK error (check cable/pins)
- `last_capture_error: 2` = Frame capture failed
- `last_capture_error: 3` = Memory allocation failed

---

## Error Handling Examples

### Camera Not Connected
```bash
$ curl http://172.20.10.2/run
{"ok":false,"error":"Camera not initialized","details":"Error 0x20004"}

$ curl http://172.20.10.2/diagnose | jq .hardware
{
  "camera_initialized": false,
  "camera_init_error": "0x20004",
  "last_capture_error": 0,
  "last_inference_ms": 0
}
```

### Successful Inference
```bash
$ curl http://172.20.10.2/run
{
  "ok": true,
  "top_label": "person",
  "top_score": 0.95,
  "inference_ms": 234,
  "labels": {"person": 0.95, "no_person": 0.05}
}
```

### Check System Status
```bash
$ curl http://172.20.10.2/health
{
  "ok": true,
  "wifi_connected": true,
  "camera_ready": false,
  "uptime_sec": 120,
  "heap_free": 2000000,
  "boot_count": 3
}
```

---

## Status Codes Reference

| Code | Meaning | When | Recovery |
|------|---------|------|----------|
| 200 | OK | Request successful | Proceed normally |
| 404 | Not Found | Resource missing | Check what you're requesting |
| 500 | Server Error | Operation failed | Check /diagnose for details |
| 503 | Unavailable | Hardware not ready | Check /health to see what's missing |

---

## Testing Workflow

### 1. Verify Device is Online
```bash
curl http://172.20.10.2/health
```

### 2. Check What's Connected
```bash
curl http://172.20.10.2/diagnose | jq .hardware
```

### 3. If Camera Missing
- Try: `curl http://172.20.10.2/run` (will return 503)
- Check connections
- Upload firmware again
- Device will detect it on next boot

### 4. If Everything Works
```bash
curl http://172.20.10.2/run
curl http://172.20.10.2/image.jpg -o image.jpg
```

---

## Python Client Example

```python
import requests

class ESP32Client:
    def __init__(self, ip="172.20.10.2"):
        self.base_url = f"http://{ip}"
        self.session = requests.Session()
    
    def health_check(self):
        """Quick health check"""
        resp = self.session.get(f"{self.base_url}/health", timeout=2)
        return resp.json()
    
    def run_inference(self):
        """Run inference with proper error handling"""
        resp = self.session.get(f"{self.base_url}/run", timeout=5)
        
        if resp.status_code == 503:
            print("Camera not available - check /diagnose")
            return None
        elif resp.status_code == 500:
            print(f"Inference error: {resp.json()}")
            return None
        elif resp.status_code == 200:
            return resp.json()
    
    def diagnose(self):
        """Get full diagnostics"""
        resp = self.session.get(f"{self.base_url}/diagnose", timeout=2)
        return resp.json()

# Usage
client = ESP32Client()
status = client.health_check()
print(f"Camera ready: {status['camera_ready']}")
print(f"WiFi connected: {status['wifi_connected']}")

if status['camera_ready']:
    result = client.run_inference()
    if result:
        print(f"Detection: {result['top_label']} ({result['top_score']:.2%})")
else:
    diag = client.diagnose()
    print(f"Camera error: {diag['hardware']['camera_init_error']}")
```

