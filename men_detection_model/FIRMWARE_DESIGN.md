# Firmware Design – ESP32 Person Classifier

## Core Principle

**The `.ino` is frozen firmware. The Python script is where all logic lives.**

Compile + upload takes ~10 minutes (VirtualBox USB). The firmware exposes a stable HTTP API that is rich enough that Python never needs a new `.ino` change for any experiment, threshold, logic change, or new feature.

---

## HTTP API  (base: `http://<ESP32_IP>`)

| Endpoint | Method | Description |
|---|---|---|
| `/info` | GET | Static metadata: labels, image dimensions, IP |
| `/run` | GET | Capture frame → run inference → store JPEG → return full JSON |
| `/capture` | GET | Capture frame → store JPEG only (no inference) |
| `/image.jpg` | GET | Return JPEG of the last frame stored by `/run` or `/capture` |

### `/info` response
```json
{
  "labels": ["no_person", "person"],
  "label_count": 2,
  "img_width": 96,
  "img_height": 96,
  "ip": "192.168.x.x"
}
```

### `/run` response
```json
{
  "ok": true,
  "labels": {"no_person": 0.0039, "person": 0.9961},
  "top": "person",
  "top_score": 0.9961,
  "inference_ms": 147,
  "dsp_ms": 2,
  "anomaly": 0.0,
  "img_width": 96,
  "img_height": 96,
  "jpg_bytes": 1453,
  "uptime_ms": 32100
}
```
> `top` and `top_score` are convenience fields so Python doesn't need to scan `labels`.  
> `uptime_ms` lets Python detect board resets.

---

## Rules — when to touch the .ino

**Never touch the .ino for:**
- Changing detection threshold (use `top_score` in Python)
- Changing save folder or filename format (Python-only)
- Adding logging, alerts, or statistics (Python-only)
- Changing JPEG quality displayed to user (Python fetches the JPEG, can re-encode)
- Supporting multiple clients / sessions (Python-only)
- Adding new labels to the model → Edge Impulse export → re-train only, API auto-expands
- Different actions on detection (buzzer, relay, etc.) → connect to GPIO pins and expose a new endpoint

**Only touch the .ino for:**
- Camera pin changes (different board)
- New model (new Edge Impulse library version)
- Hardware GPIO endpoints (e.g. `/led` to control an LED)
- PSRAM / arena size changes

---

## Compile & Upload Commands

```bash
export PATH="$HOME/.local/bin:$PATH"
SKETCH="/home/amit/studies/final_project/Final-Project/men_detection_model/men_ai_model"
FQBN="esp32:esp32:esp32s3:PSRAM=opi,FlashSize=4M,PartitionScheme=huge_app,USBMode=hwcdc,UploadMode=default,CPUFreq=240"
CFG="$HOME/.arduinoIDE/arduino-cli.yaml"

arduino-cli compile --fqbn "$FQBN" --config-file "$CFG" "$SKETCH" && \
arduino-cli upload  --fqbn "$FQBN" --port /dev/ttyUSB0 \
  --config-file "$CFG" --upload-field upload.speed=115200 "$SKETCH"
```

---

## Get ESP32 IP After Boot

```bash
python3 -c "
import serial, sys
s = serial.Serial('/dev/ttyUSB0', 460800, timeout=20)
for _ in range(100):
    l = s.readline().decode(errors='replace').strip()
    print(l)
    if 'IP:' in l: break
s.close()
"
```

---

## Serial Port (debug only)

Serial runs at **460800 baud**. Commands for hardware debugging:
- `CAPTURE` — capture frame only (prints debug info)
- `RUN` — capture + inference, prints text results to serial
- `TESTMODEL` — run inference on a black dummy image (verifies TFLite without camera)

Serial is **not** used for image transfer. All real usage goes through WiFi HTTP.

---

## Critical Library Fixes (do not revert)

| File | Change | Why |
|---|---|---|
| `src/tflite-model/tflite_learn_1013165_3.h` | arena_size = 700000 | Default 256KB too small for this SDK |
| `src/edge-impulse-sdk/tensorflow/lite/micro/micro_log.h` | `TF_LITE_STRIP_ERROR_STRINGS` commented out | Was silencing all TFLite errors |
| `men_ai_model.ino` | `ei_malloc`/`ei_calloc` overridden to `MALLOC_CAP_SPIRAM` | Internal RAM only has ~29KB free; arena needs 700KB |

---

## WiFi Credentials

- SSID: `Amit_wifi`
- Password: `Amit1998`

To change: edit `WIFI_SSID` / `WIFI_PASSWORD` at top of `men_ai_model.ino` and re-upload.
