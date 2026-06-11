# Project Roadmap — ESP32-S3 Person Detector

> **Rule**: Update this file at the end of every session or after every major improvement.
>
> For technical reference → see [FIRMWARE_DESIGN.md](FIRMWARE_DESIGN.md)  
> For debug history → see [men_ai_model/SESSION_NOTES.md](men_ai_model/SESSION_NOTES.md)

---

## Final Goal

A low-power smart sensor:

1. ESP32-S3 runs in **deep sleep** to save power
2. **PIR / motion sensors** stay active during sleep (ultra-low power)
3. Sensor detects movement → **wakes the ESP32**
4. Camera captures a frame → **AI model classifies**: person / no_person (~148ms)
5. If **person detected** → sends image + alert over **WiFi** to PC/phone

---

## Current Status

| # | Feature | Status | Notes |
|---|---------|--------|-------|
| 1 | AI model runs on ESP32 | ✅ Done | ~148ms inference |
| 2 | WiFi HTTP server + endpoints | ✅ Done | /run /capture /image.jpg /info |
| 3 | Python client saves labeled JPEG | ✅ Done | `captured_images/YYYYMMDD_label_XX%.jpg` |
| 4 | Display image quality | ✅ Done | 320×240 px, quality 60, ~8KB |
| 5 | Auto-route healing (VM networking) | ✅ Done | Python calls `_ensure_route()` on startup |
| 6 | Increase capture resolution | ⚠️ Pending | Currently QVGA — bump to VGA/SVGA |
| 7 | Retrain model on real camera images | ⚠️ Pending | Current model trained on generic dataset |
| 8 | Deep sleep + sensor wakeup | ❌ Not started | Code skeleton in `sleep mode/` folder |
| 9 | WiFi push alert on detection | ❌ Not started | POST to VM Python server |

---

## Known Limitations & Issues

### Image quality / model accuracy
- Model input = **96×96 px** (fixed by Edge Impulse export)
- Camera configured for QVGA (320×240), crops center 96×96 for inference
- Real-world accuracy is limited because model was trained on a generic dataset, not on this exact camera
- **Fix**: Increase frame size to VGA/SVGA (more context for the crop) + eventually retrain

### ESP32 IP may change
- Phone hotspot (Amit_wifi) assigns IPs via DHCP — IP might change after reboot
- Current hardcoded IP in `men_ai_model.py`: `172.20.10.2`
- **Fix if broken**: Read serial at 460800 baud after ESP32 boot to find new IP

### VM route drops after NM reconnect
- Bridged NIC (enp0s8) loses kernel route when NetworkManager reconnects
- **Mitigated**: Python `_ensure_route()` re-adds route automatically on every run

---

## Improvement Roadmap

### Step 1 — Better camera resolution (easy, 2-line .ino change)
Change in `men_ai_model.ino` → `initCamera()`:
```cpp
// Change:
config.frame_size = FRAMESIZE_QVGA;  // 320×240
// To:
config.frame_size = FRAMESIZE_VGA;   // 640×480 — larger frame, same 96×96 crop
```
Benefit: bigger surrounding context → better inference accuracy + bigger saved image.

### Step 2 — Deep sleep + sensor wakeup
```cpp
#define PIR_PIN GPIO_NUM_X   // connect PIR OUT to this GPIO
esp_sleep_enable_ext0_wakeup(PIR_PIN, 1);
esp_deep_sleep_start();
```
Wake-on-HIGH from PIR → run inference → if person: send alert → sleep again.
Reference skeleton: `sleep mode/sleep_mode/sleep_mode.ino`

### Step 3 — WiFi push alert on detection
ESP32 POSTs to a Python HTTP server on VM when person detected:
```cpp
HTTPClient http;
http.begin("http://172.20.10.3:8080/alert");
http.addHeader("Content-Type", "application/json");
http.POST("{\"person\":true,\"score\":0.98}");
http.end();
```
Python: simple `http.server` or Flask listener saves image and shows desktop notification.

### Step 4 — Retrain model
1. Use `men_detection_model/Capturing/` scripts to collect frames from this camera
2. Upload to Edge Impulse, label, retrain at 96×96
3. Export as Arduino library → replace `esp32_person_classifier_inferencing`

---

## Session Log

### 2026-06-11 — Session 1+2
**What was done:**
- Fixed TFLite arena error -3 (arena 256KB → 700KB in PSRAM, MALLOC_CAP_SPIRAM override)
- Enabled TFLite error strings (commented out TF_LITE_STRIP_ERROR_STRINGS)
- Switched from serial image transfer (broken in VirtualBox USB) to WiFi HTTP API
- Solved VM routing issue (bridged adapter enp0s8 needs manual route; Python auto-heals)
- Upgraded display image: 96×96 quality-15 (1.7KB) → 320×240 quality-60 (8.3KB)
- Python client working end-to-end: Enter → capture → inference → save labeled JPEG

**Detection rate observed:**
- Only 1 test image taken: `person 98.4%` / `no_person 1.6%`
- Too few samples to judge real-world accuracy — need more captures in varied conditions
- Photo quality is now much better (320×240) but model still limited to 96×96 crop

**Blocked:**
- Sleep mode & sensor wakeup not yet started
- Auto WiFi alert not yet started
