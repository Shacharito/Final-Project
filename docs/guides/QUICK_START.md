# Quick Start Guide - Robust Firmware

## 30-Second Overview

✅ **What's New:**
- Device won't crash if camera is missing
- New `/health` and `/diagnose` endpoints for troubleshooting
- Clear error messages instead of mysterious failures
- Works with partial hardware

✅ **When to Use This:**
- Sensors not connected yet? Device will still run!
- Want to see device status? Use `/health`אי
- Need detailed diagnostics? Use `/diagnose`

---

## 5-Minute Setup

### Step 1: Power Cycle Device (1 min)
```bash
# Disconnect USB from ESP32
# Wait 3 seconds
# Reconnect USB
```

### Step 2: Compile (1 min)
```bash
cd /home/amit/studies/final_project/Final-Project

FQBN="esp32:esp32:esp32s3:PartitionScheme=huge_app"
arduino-cli compile --fqbn $FQBN firmware/main/main.ino
```

Expected output:
```
Sketch uses 1746384 bytes (55%) of program storage space.
```

### Step 3: Upload (1 min)
```bash
arduino-cli upload -p /dev/ttyUSB0 --fqbn $FQBN firmware/main/main.ino
```

### Step 4: Test (2 min)
```bash
# Wait for device to boot (~3 seconds)
sleep 3

# Quick check
curl http://172.20.10.2/health | jq

# Should see:
# {
#   "ok": true,
#   "wifi_connected": true,
#   "camera_ready": false,  ← Note: missing camera is OK!
#   ...
# }
```

---

## Common Scenarios

### Scenario 1: Camera Missing (No Problem!)

**What you'll see:**
```bash
$ curl http://172.20.10.2/health
{
  "wifi_connected": true,
  "camera_ready": false,  ← Sensor missing, device OK
  "uptime_sec": 45
}
```

**What to do:**
```bash
# Check error details
curl http://172.20.10.2/diagnose | jq .hardware

# Result might be:
{
  "camera_initialized": false,
  "camera_init_error": "0x20004",  ← XCLK pin issue
}

# Action: Connect camera and reboot
```

**Once camera is connected:**
```bash
# Just reboot device - no code changes needed!
# Device auto-detects on boot

# Verify it worked:
curl http://172.20.10.2/health
# Should now show: camera_ready: true
```

---

### Scenario 2: Everything Works

**What you'll see:**
```bash
$ curl http://172.20.10.2/health
{
  "wifi_connected": true,
  "camera_ready": true,  ← Everything ready!
  "uptime_sec": 45
}
```

**Run inference:**
```bash
curl http://172.20.10.2/run | jq

# Returns:
{
  "ok": true,
  "top_label": "person",
  "top_score": 0.95,
  "inference_ms": 234
}
```

---

### Scenario 3: Need Full Diagnostics

```bash
# Get everything at once
curl http://172.20.10.2/diagnose | jq

# Shows:
{
  "device_info": {
    "mac_address": "3C:0F:02:D7:56:C4",
    "local_ip": "172.20.10.2",
    "signal_strength": -45
  },
  "hardware": {
    "camera_initialized": false,
    "camera_init_error": "0x20004"  ← Shows exact error
  },
  "memory": {
    "heap_free": 1234567,
    "psram_free": 8000000
  }
}
```

---

## API Cheat Sheet

```bash
# Health check (fast)
curl http://172.20.10.2/health

# Full diagnostics
curl http://172.20.10.2/diagnose | jq

# Run inference
curl http://172.20.10.2/run

# Get last image
curl http://172.20.10.2/image.jpg -o image.jpg

# Get configuration
curl http://172.20.10.2/get_config

# Update configuration
curl -X POST "http://172.20.10.2/set_config?sleep_sec=120"

# Put device to sleep
curl -X POST "http://172.20.10.2/sleep?duration=300"
```

---

## Error Codes

| Code | Meaning | Action |
|------|---------|--------|
| `0x20001` | I2C error | Check camera connector |
| `0x20004` | XCLK error | Check power to camera |
| `503` | Camera not ready | Connect camera & reboot |
| `500` | Inference failed | Check memory, try again |

---

## Troubleshooting

### Problem: Device shows `camera_ready: false`

**Solution:**
```bash
# Step 1: Get error code
curl http://172.20.10.2/diagnose | jq '.hardware.camera_init_error'

# Step 2: Check what it means
# 0x20004 = XCLK issue (check power)
# 0x20001 = I2C error (check data lines)

# Step 3: Fix hardware connection
# Then reboot device
```

### Problem: Device doesn't respond to `/run`

**Solution:**
```bash
# Check if camera is ready
curl http://172.20.10.2/health | jq '.camera_ready'

# If false: Connect camera first
# If true: Check error details
curl http://172.20.10.2/run | jq '.error'
```

### Problem: Device not responding to anything

**Solution:**
```bash
# Check if device is online
ping 172.20.10.2

# If not online:
# 1. Power cycle device
# 2. Check USB connection
# 3. Wait 5 seconds for boot
# 4. Try /health again

# If online but /health fails:
# Device may be in reboot loop
# Try different USB port
```

---

## Python Client Example

```python
import requests

# Initialize client
base_url = "http://172.20.10.2"

# Check health
health = requests.get(f"{base_url}/health").json()
print(f"Camera ready: {health['camera_ready']}")

# If camera ready, run inference
if health['camera_ready']:
    result = requests.get(f"{base_url}/run").json()
    print(f"Detection: {result['top_label']} ({result['top_score']:.2%})")
else:
    # Get diagnostics to see what's wrong
    diag = requests.get(f"{base_url}/diagnose").json()
    error_code = diag['hardware']['camera_init_error']
    print(f"Camera error: {error_code}")
```

---

## Documentation Files

**Quick Reference:**
- This file (QUICK_START.md) - Start here
- `API_REFERENCE.md` - All endpoints explained

**Deep Dive:**
- `ARCHITECTURE_DIAGRAM.md` - Visual architecture
- `ROBUST_FIRMWARE_GUIDE.md` - Design details
- `IMPLEMENTATION_SUMMARY.md` - What changed

**Troubleshooting:**
- `ROOT_CAUSE_ANALYSIS.md` - Original issue explained
- `HARDWARE_DIAGNOSTIC.md` - Hardware tests

---

## Success Checklist

After upload, verify:

- [ ] Device boots without crashing
- [ ] `/health` returns 200 with JSON
- [ ] Device shows up in `/health` response
- [ ] Can see `wifi_connected` and `camera_ready` fields
- [ ] `/diagnose` returns full hardware details
- [ ] Device stays online (doesn't sleep unexpectedly)

---

## Next Steps

### If Camera is Missing
```bash
# Device will still work! Just:
1. Check /health → confirms camera missing
2. Check /diagnose → see error code
3. Connect camera to proper pins
4. Reboot device (power cycle)
5. Device auto-detects, no code changes needed
```

### If Everything Works
```bash
# Run inference!
1. curl /run → get person detection results
2. curl /image.jpg → download captured frame
3. Use Python client for continuous monitoring
```

### For Production
```bash
# Deploy with confidence:
1. Device handles missing sensors gracefully
2. Can monitor via /health endpoint
3. Can diagnose issues via /diagnose
4. Can update sensors anytime - no recompile needed
```

---

## One-Liner Tests

```bash
# Quick online check
curl -s http://172.20.10.2/health | grep -q '"ok":true' && echo "✅ Online" || echo "❌ Offline"

# Quick camera check
curl -s http://172.20.10.2/health | grep -q '"camera_ready":true' && echo "✅ Camera OK" || echo "⚠️ Camera missing"

# Run inference and show result
curl -s http://172.20.10.2/run | jq '.top_label, .top_score'

# Monitor continuously
watch -n 1 'curl -s http://172.20.10.2/health | jq "{wifi: .wifi_connected, camera: .camera_ready, uptime: .uptime_sec}"'
```

---

## Need Help?

1. **Device won't boot?** → Check POWER and USB connection
2. **Can't connect to WiFi?** → Check SSID/password in /get_config
3. **Camera not detected?** → Check /diagnose for error code
4. **Inference failing?** → Run /run again (may be transient)
5. **Memory low?** → Check /diagnose for heap_free

**Still stuck?** Check `ROOT_CAUSE_ANALYSIS.md` for detailed troubleshooting.

---

## You're Ready! 🚀

The firmware is robust and ready to handle real-world conditions. Whether sensors are connected or not, the device will:
- ✅ Boot reliably
- ✅ Report its status
- ✅ Continue running
- ✅ Provide diagnostics

**Proceed with confidence!**

