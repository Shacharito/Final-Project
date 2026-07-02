#include "esp_heap_caps.h"
#include <esp32_person_classifier_inferencing.h>
#include "esp_camera.h"
#include "img_converters.h"
#include <stdarg.h>
#include <WiFi.h>
#include <WebServer.h>

const char *WIFI_SSID     = "Amit_wifi";
const char *WIFI_PASSWORD = "Amit1998";

WebServer server(80);
static uint8_t *jpg_buf = nullptr;
static size_t   jpg_len = 0;

// Override ei_printf so all Edge Impulse internal errors appear on Serial
void ei_printf(const char *format, ...) {
    char buf[256];
    va_list args;
    va_start(args, format);
    vsnprintf(buf, sizeof(buf), format, args);
    va_end(args);
    Serial.print(buf);
    Serial.flush();
}

// Force TFLite arena into PSRAM (internal heap only has ~29KB free, arena needs 256KB)
void *ei_malloc(size_t size) {
    return heap_caps_malloc(size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
}

void *ei_calloc(size_t nitems, size_t size) {
    return heap_caps_calloc(nitems, size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
}

void ei_free(void *ptr) {
    free(ptr);
}

// =========================
// GOOUUU ESP32 S3 CAM pins
// =========================

#define PWDN_GPIO_NUM     -1
#define RESET_GPIO_NUM    -1

#define XCLK_GPIO_NUM     15
#define SIOD_GPIO_NUM      4
#define SIOC_GPIO_NUM      5

#define Y9_GPIO_NUM       16
#define Y8_GPIO_NUM       17
#define Y7_GPIO_NUM       18
#define Y6_GPIO_NUM       12
#define Y5_GPIO_NUM       10
#define Y4_GPIO_NUM        8
#define Y3_GPIO_NUM        9
#define Y2_GPIO_NUM       11

#define VSYNC_GPIO_NUM     6
#define HREF_GPIO_NUM      7
#define PCLK_GPIO_NUM     13

#define IMG_WIDTH   EI_CLASSIFIER_INPUT_WIDTH
#define IMG_HEIGHT  EI_CLASSIFIER_INPUT_HEIGHT

static uint8_t *snapshot_buf = nullptr;

void debugPrint(const char *msg) {
  Serial.println(msg);
  Serial.flush();
}

void printEiError(EI_IMPULSE_ERROR error) {
  int error_code = (int)error;

  Serial.print("EI error code: ");
  Serial.println(error_code);

  if (error_code == 0) {
    Serial.println("EI_IMPULSE_OK");
  }
  else if (error_code == -1) {
    Serial.println("Possible error: shapes do not match");
  }
  else if (error_code == -2) {
    Serial.println("Possible error: impulse canceled");
  }
  else if (error_code == -3) {
    Serial.println("Possible error: TFLite or inference engine error");
  }
  else if (error_code == -4) {
    Serial.println("Possible error: DSP or preprocessing error");
  }
  else {
    Serial.println("Unknown Edge Impulse error code");
  }

  Serial.flush();
}

void rgb565_to_rgb888(uint16_t pixel, uint8_t *r, uint8_t *g, uint8_t *b) {
  *r = ((pixel >> 11) & 0x1F) << 3;
  *g = ((pixel >> 5) & 0x3F) << 2;
  *b = (pixel & 0x1F) << 3;
}

void initCamera() {
  debugPrint("CAMERA INIT 1: start");

  camera_config_t config;

  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer = LEDC_TIMER_0;

  config.pin_d0 = Y2_GPIO_NUM;
  config.pin_d1 = Y3_GPIO_NUM;
  config.pin_d2 = Y4_GPIO_NUM;
  config.pin_d3 = Y5_GPIO_NUM;
  config.pin_d4 = Y6_GPIO_NUM;
  config.pin_d5 = Y7_GPIO_NUM;
  config.pin_d6 = Y8_GPIO_NUM;
  config.pin_d7 = Y9_GPIO_NUM;

  config.pin_xclk = XCLK_GPIO_NUM;
  config.pin_pclk = PCLK_GPIO_NUM;
  config.pin_vsync = VSYNC_GPIO_NUM;
  config.pin_href = HREF_GPIO_NUM;

  config.pin_sccb_sda = SIOD_GPIO_NUM;
  config.pin_sccb_scl = SIOC_GPIO_NUM;

  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;

  config.xclk_freq_hz = 20000000;
  config.pixel_format = PIXFORMAT_RGB565;
  config.frame_size = FRAMESIZE_QVGA;

  config.jpeg_quality = 10;
  config.fb_count = 1;

  if (psramFound()) {
    config.fb_location = CAMERA_FB_IN_PSRAM;
    config.grab_mode = CAMERA_GRAB_LATEST;
  } else {
    config.fb_location = CAMERA_FB_IN_DRAM;
    config.grab_mode = CAMERA_GRAB_WHEN_EMPTY;
  }

  debugPrint("CAMERA INIT 2: before esp_camera_init");

  esp_err_t err = esp_camera_init(&config);

  debugPrint("CAMERA INIT 3: after esp_camera_init");

  if (err != ESP_OK) {
    Serial.printf("Camera init failed: 0x%x\n", err);
    Serial.flush();

    while (true) {
      delay(1000);
    }
  }

  sensor_t *s = esp_camera_sensor_get();

  if (s == nullptr) {
    debugPrint("Camera sensor pointer is NULL");

    while (true) {
      delay(1000);
    }
  }

  debugPrint("CAMERA INIT 4: before sensor settings");

  s->set_brightness(s, 0);
  s->set_contrast(s, 1);
  s->set_saturation(s, 0);
  s->set_sharpness(s, 1);
  s->set_denoise(s, 1);
  s->set_gainceiling(s, GAINCEILING_4X);

  debugPrint("Camera ready");
}

bool captureImageForModel() {
  debugPrint("CAPTURE A: before esp_camera_fb_get");

  camera_fb_t *fb = esp_camera_fb_get();

  debugPrint("CAPTURE B: after esp_camera_fb_get");

  if (!fb) {
    debugPrint("Capture failed: fb is NULL");
    return false;
  }

  if (!fb->buf) {
    debugPrint("Capture failed: fb buf is NULL");
    esp_camera_fb_return(fb);
    return false;
  }

  if (!snapshot_buf) {
    debugPrint("Capture failed: snapshot_buf is NULL");
    esp_camera_fb_return(fb);
    return false;
  }

  int src_width = fb->width;
  int src_height = fb->height;

  Serial.printf("Captured frame: %d x %d\n", src_width, src_height);
  Serial.flush();

  Serial.print("fb length: ");
  Serial.println(fb->len);
  Serial.flush();

  if (src_width <= 0 || src_height <= 0) {
    debugPrint("Capture failed: invalid frame size");
    esp_camera_fb_return(fb);
    return false;
  }

  int crop_size = src_width < src_height ? src_width : src_height;

  int crop_x0 = (src_width - crop_size) / 2;
  int crop_y0 = (src_height - crop_size) / 2;

  Serial.print("Crop size: ");
  Serial.println(crop_size);

  Serial.print("Crop x0: ");
  Serial.println(crop_x0);

  Serial.print("Crop y0: ");
  Serial.println(crop_y0);

  Serial.print("IMG_WIDTH: ");
  Serial.println(IMG_WIDTH);

  Serial.print("IMG_HEIGHT: ");
  Serial.println(IMG_HEIGHT);

  Serial.print("Expected snapshot bytes: ");
  Serial.println(IMG_WIDTH * IMG_HEIGHT * 3);

  Serial.flush();

  debugPrint("CAPTURE C: before conversion loop");

  uint16_t *src = (uint16_t *)fb->buf;

  for (int y = 0; y < IMG_HEIGHT; y++) {
    for (int x = 0; x < IMG_WIDTH; x++) {

      int src_x = crop_x0 + (x * crop_size) / IMG_WIDTH;
      int src_y = crop_y0 + (y * crop_size) / IMG_HEIGHT;

      uint16_t pixel = src[src_y * src_width + src_x];

      uint8_t r, g, b;
      rgb565_to_rgb888(pixel, &r, &g, &b);

      int dst_index = (y * IMG_WIDTH + x) * 3;

      snapshot_buf[dst_index + 0] = r;
      snapshot_buf[dst_index + 1] = g;
      snapshot_buf[dst_index + 2] = b;
    }
  }

  debugPrint("CAPTURE D: after conversion loop");

  esp_camera_fb_return(fb);

  debugPrint("CAPTURE E: frame returned");

  return true;
}

int get_data(size_t offset, size_t length, float *out_ptr) {
  if (!snapshot_buf) {
    return -1;
  }

  size_t pixel_ix = offset * 3;

  for (size_t i = 0; i < length; i++) {
    uint8_t r = snapshot_buf[pixel_ix + 0];
    uint8_t g = snapshot_buf[pixel_ix + 1];
    uint8_t b = snapshot_buf[pixel_ix + 2];

    out_ptr[i] = (r << 16) + (g << 8) + b;

    pixel_ix += 3;
  }

  return 0;
}

int get_dummy_data(size_t offset, size_t length, float *out_ptr) {
  static int call_count = 0;

  if (call_count < 5) {
    Serial.print("get_dummy_data called. offset=");
    Serial.print(offset);
    Serial.print(", length=");
    Serial.println(length);
    call_count++;
  }

  for (size_t i = 0; i < length; i++) {
    out_ptr[i] = 0x000000;
  }

  return 0;
}

void printResults(ei_impulse_result_t result) {
  debugPrint("PRINT A: before printing results");

  Serial.println("Inference results:");
  Serial.flush();

  for (int i = 0; i < EI_CLASSIFIER_LABEL_COUNT; i++) {
    Serial.printf("%s: %.3f\n",
                  result.classification[i].label,
                  result.classification[i].value);
    Serial.flush();
  }

  Serial.printf("DSP: %d ms\n", result.timing.dsp);
  Serial.printf("Classification: %d ms\n", result.timing.classification);
  Serial.printf("Anomaly: %d ms\n", result.timing.anomaly);
  Serial.println();
  Serial.flush();

  debugPrint("PRINT B: after printing results");
}

void printModelInfo() {
  Serial.print("Model input width: ");
  Serial.println(IMG_WIDTH);

  Serial.print("Model input height: ");
  Serial.println(IMG_HEIGHT);

  Serial.print("Label count: ");
  Serial.println(EI_CLASSIFIER_LABEL_COUNT);

#if EI_CLASSIFIER_OBJECT_DETECTION == 1
  Serial.println("Model type: Object Detection");
#else
  Serial.println("Model type: Classification");
#endif

  Serial.print("Compiled: ");
  Serial.println(EI_CLASSIFIER_COMPILED);

  Serial.print("Quantized: ");
  Serial.println(EI_CLASSIFIER_QUANTIZATION_ENABLED);

  Serial.print("DSP input frame size: ");
  Serial.println(EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE);

  Serial.print("Raw sample count: ");
  Serial.println(EI_CLASSIFIER_RAW_SAMPLE_COUNT);

  Serial.print("NN input frame size: ");
  Serial.println(EI_CLASSIFIER_NN_INPUT_FRAME_SIZE);

  Serial.print("TFLite largest arena size: ");
  Serial.println(EI_CLASSIFIER_TFLITE_LARGEST_ARENA_SIZE);

  Serial.print("Signal length used by code: ");
  Serial.println(EI_CLASSIFIER_RAW_SAMPLE_COUNT);

  Serial.flush();
}

// ── WiFi + HTTP handlers ──────────────────────────────────────────────────────

void setupWiFi() {
  Serial.printf("Connecting to WiFi: %s\n", WIFI_SSID);
  Serial.flush();
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  int tries = 0;
  while (WiFi.status() != WL_CONNECTED && tries < 30) {
    delay(500);
    Serial.print(".");
    tries++;
  }
  Serial.println();
  if (WiFi.status() == WL_CONNECTED) {
    Serial.printf("WiFi connected!  IP: %s\n", WiFi.localIP().toString().c_str());
    Serial.printf("Endpoints:\n  http://%s/run\n  http://%s/image.jpg\n",
                  WiFi.localIP().toString().c_str(),
                  WiFi.localIP().toString().c_str());
  } else {
    Serial.println("WiFi FAILED - continuing without network");
  }
  Serial.flush();
}

// ── helpers ──────────────────────────────────────────────────────────────────

// Capture a fresh full-resolution (320x240) frame and store as JPEG quality 60.
// This is purely for human viewing — it does not affect model inference at all.
static void captureDisplayJpeg() {
  camera_fb_t *fb = esp_camera_fb_get();
  if (!fb) return;
  if (jpg_buf) { free(jpg_buf); jpg_buf = nullptr; jpg_len = 0; }
  // Convert full 320x240 RGB565 frame → JPEG at quality 60
  fmt2jpg(fb->buf, fb->len, fb->width, fb->height, PIXFORMAT_RGB565, 60, &jpg_buf, &jpg_len);
  esp_camera_fb_return(fb);
}

// Find the label index with highest score
static int topLabel(ei_impulse_result_t &result) {
  int best = 0;
  for (int i = 1; i < EI_CLASSIFIER_LABEL_COUNT; i++)
    if (result.classification[i].value > result.classification[best].value)
      best = i;
  return best;
}

// ── HTTP handlers ─────────────────────────────────────────────────────────────

// GET /info  →  static model & board metadata (never changes without a reflash)
void handleInfo() {
  String json = "{";
  json += "\"labels\":[";
  for (int i = 0; i < EI_CLASSIFIER_LABEL_COUNT; i++) {
    if (i) json += ",";
    json += "\"";
    json += ei_classifier_inferencing_categories[i];
    json += "\"";
  }
  json += "],";
  json += "\"label_count\":" + String(EI_CLASSIFIER_LABEL_COUNT) + ",";
  json += "\"img_width\":"  + String(IMG_WIDTH)  + ",";
  json += "\"img_height\":" + String(IMG_HEIGHT) + ",";
  json += "\"ip\":\""       + WiFi.localIP().toString() + "\"";
  json += "}";
  server.send(200, "application/json", json);
}

// GET /run  →  capture + inference + store JPEG; returns rich JSON
void handleRun() {
  if (!captureImageForModel()) {
    server.send(500, "application/json", "{\"ok\":false,\"error\":\"capture failed\"}");
    return;
  }

  signal_t signal;
  signal.total_length = EI_CLASSIFIER_RAW_SAMPLE_COUNT;
  signal.get_data = &get_data;
  ei_impulse_result_t result = {0};

  if (run_classifier(&signal, &result, false) != EI_IMPULSE_OK) {
    server.send(500, "application/json", "{\"ok\":false,\"error\":\"inference failed\"}");
    return;
  }

  captureDisplayJpeg();

  int top = topLabel(result);

  // Rich JSON — add any new field here on the Python side without touching .ino
  String json = "{";
  json += "\"ok\":true,";
  // per-label scores
  json += "\"labels\":{";
  for (int i = 0; i < EI_CLASSIFIER_LABEL_COUNT; i++) {
    if (i) json += ",";
    json += "\"";
    json += result.classification[i].label;
    json += "\":";
    json += String(result.classification[i].value, 4);
  }
  json += "},";
  // convenience top-level fields
  json += "\"top\":\""         + String(result.classification[top].label) + "\",";
  json += "\"top_score\":"      + String(result.classification[top].value, 4) + ",";
  json += "\"inference_ms\":"   + String(result.timing.classification) + ",";
  json += "\"dsp_ms\":"         + String(result.timing.dsp) + ",";
  json += "\"anomaly\":"        + String(result.timing.anomaly) + ",";
  json += "\"img_width\":"      + String(IMG_WIDTH) + ",";
  json += "\"img_height\":"     + String(IMG_HEIGHT) + ",";
  json += "\"jpg_bytes\":"      + String((int)jpg_len) + ",";
  json += "\"uptime_ms\":"      + String(millis());
  json += "}";

  server.send(200, "application/json", json);
  Serial.printf("[/run] %s %.0f%%  %dms  jpg=%dB\n",
                result.classification[top].label,
                result.classification[top].value * 100,
                result.timing.classification, (int)jpg_len);
  Serial.flush();
}

// GET /capture  →  capture JPEG only (no inference); useful for camera testing
void handleCapture() {
  if (!captureImageForModel()) {
    server.send(500, "application/json", "{\"ok\":false,\"error\":\"capture failed\"}");
    return;
  }
  captureDisplayJpeg();
  String json = "{\"ok\":true,\"jpg_bytes\":" + String((int)jpg_len) + "}";
  server.send(200, "application/json", json);
}

// GET /image.jpg  →  JPEG of last frame (from /run or /capture)
void handleImage() {
  if (!jpg_buf || jpg_len == 0) {
    server.send(404, "application/json", "{\"ok\":false,\"error\":\"no image yet\"}" );
    return;
  }
  server.send_P(200, "image/jpeg", (const char *)jpg_buf, jpg_len);
}

// ─────────────────────────────────────────────────────────────────────────────

void setup() {
  Serial.begin(460800);
  delay(3000);

  Serial.println();
  Serial.println("=== ESP32 S3 Human Detection System DEBUG ===");
  Serial.flush();

  printModelInfo();

  if (psramFound()) {
    debugPrint("PSRAM found");
  } else {
    debugPrint("PSRAM not found");
  }

  debugPrint("SETUP A: before snapshot buffer allocation");

  size_t snapshot_size = IMG_WIDTH * IMG_HEIGHT * 3;

  Serial.print("Snapshot buffer size: ");
  Serial.println(snapshot_size);

  Serial.print("Free internal heap: ");
  Serial.println(heap_caps_get_free_size(MALLOC_CAP_INTERNAL));

  Serial.print("Free PSRAM: ");
  Serial.println(heap_caps_get_free_size(MALLOC_CAP_SPIRAM));

  Serial.flush();

  snapshot_buf = (uint8_t *)heap_caps_malloc(snapshot_size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);

  if (snapshot_buf == nullptr) {
    debugPrint("PSRAM allocation failed, trying regular malloc");

    snapshot_buf = (uint8_t *)malloc(snapshot_size);
  }

  if (snapshot_buf == nullptr) {
    debugPrint("Failed to allocate snapshot buffer");

    while (true) {
      delay(1000);
    }
  }

  debugPrint("SETUP B: snapshot buffer allocated successfully");

  debugPrint("SETUP C: before initCamera");

  initCamera();

  debugPrint("SETUP D: after initCamera");

  setupWiFi();
  server.on("/info",      HTTP_GET, handleInfo);
  server.on("/run",       HTTP_GET, handleRun);
  server.on("/capture",   HTTP_GET, handleCapture);
  server.on("/image.jpg", HTTP_GET, handleImage);
  server.begin();
  Serial.println("HTTP server started");
  Serial.flush();

  debugPrint("System ready");

  Serial.println();
  Serial.println("Send CAPTURE to test camera only");
  Serial.println("Send RUN to capture and run inference");
  Serial.println("Send TESTMODEL to test model with dummy black image");
  Serial.flush();
}

void loop() {
  server.handleClient();
  if (Serial.available()) {
    String command = Serial.readStringUntil('\n');
    command.trim();

    if (command == "CAPTURE") {
      debugPrint("COMMAND RECEIVED: CAPTURE");

      debugPrint("TEST 1: before captureImageForModel");

      bool ok = captureImageForModel();

      debugPrint("TEST 2: after captureImageForModel");

      if (!ok) {
        debugPrint("Image capture failed");
        return;
      }

      debugPrint("TEST 3: image capture and conversion succeeded");
    }

    else if (command == "RUN") {
      bool ok = captureImageForModel();

      if (!ok) {
        Serial.println("ERROR: Image capture failed");
        Serial.flush();
        return;
      }

      signal_t signal;
      signal.total_length = EI_CLASSIFIER_RAW_SAMPLE_COUNT;
      signal.get_data = &get_data;

      ei_impulse_result_t result = {0};

      EI_IMPULSE_ERROR res = run_classifier(&signal, &result, false);

      if (res != EI_IMPULSE_OK) {
        Serial.println("ERROR: Classifier failed");
        printEiError(res);
        return;
      }

      // Results to serial (debug only — main interface is WiFi HTTP)
      Serial.println("RESULT_BEGIN");
      for (int i = 0; i < EI_CLASSIFIER_LABEL_COUNT; i++) {
        Serial.printf("%s: %.3f\n",
                      result.classification[i].label,
                      result.classification[i].value);
      }
      Serial.printf("inference_ms: %d\n", result.timing.classification);
      Serial.println("RESULT_END");
      Serial.flush();
    }

    else if (command == "TESTMODEL") {
      debugPrint("COMMAND RECEIVED: TESTMODEL");
      debugPrint("Testing model with dummy black image");

      signal_t signal;
      signal.total_length = EI_CLASSIFIER_RAW_SAMPLE_COUNT;
      signal.get_data = &get_dummy_data;

      Serial.print("Dummy signal length: ");
      Serial.println(signal.total_length);
      Serial.flush();

      ei_impulse_result_t result = {0};

      debugPrint("TESTMODEL 1: before run_classifier");

      EI_IMPULSE_ERROR res = run_classifier(&signal, &result, true);

      debugPrint("TESTMODEL 2: after run_classifier");

      if (res != EI_IMPULSE_OK) {
        Serial.println("Dummy model test failed");
        printEiError(res);
        return;
      }

      Serial.println("Dummy model test succeeded");
      printResults(result);
    }

    else if (command == "PHOTO") {
      // Capture a JPEG and stream it over serial as base64 using an ACK
      // handshake so the host serial buffer never overruns (reliable even
      // over VM USB-serial). No WiFi needed.
      if (!captureImageForModel()) {
        Serial.println("PHOTO_ERROR: capture failed");
        Serial.flush();
        return;
      }
      captureDisplayJpeg();

      if (!jpg_buf || jpg_len == 0) {
        Serial.println("PHOTO_ERROR: no jpeg");
        Serial.flush();
        return;
      }

      static const char b64[] =
          "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

      // Encode the whole JPEG to base64 in a PSRAM buffer first.
      size_t b64_len = ((jpg_len + 2) / 3) * 4;
      char *enc = (char *)heap_caps_malloc(b64_len + 1,
                                           MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
      if (enc == nullptr) {
        Serial.println("PHOTO_ERROR: base64 alloc failed");
        Serial.flush();
        return;
      }

      size_t o = 0;
      size_t i = 0;
      while (i + 2 < jpg_len) {
        uint32_t n = (jpg_buf[i] << 16) | (jpg_buf[i + 1] << 8) | jpg_buf[i + 2];
        enc[o++] = b64[(n >> 18) & 0x3F];
        enc[o++] = b64[(n >> 12) & 0x3F];
        enc[o++] = b64[(n >> 6) & 0x3F];
        enc[o++] = b64[n & 0x3F];
        i += 3;
      }
      size_t rem = jpg_len - i;
      if (rem == 1) {
        uint32_t n = jpg_buf[i] << 16;
        enc[o++] = b64[(n >> 18) & 0x3F];
        enc[o++] = b64[(n >> 12) & 0x3F];
        enc[o++] = '=';
        enc[o++] = '=';
      } else if (rem == 2) {
        uint32_t n = (jpg_buf[i] << 16) | (jpg_buf[i + 1] << 8);
        enc[o++] = b64[(n >> 18) & 0x3F];
        enc[o++] = b64[(n >> 12) & 0x3F];
        enc[o++] = b64[(n >> 6) & 0x3F];
        enc[o++] = '=';
      }
      enc[o] = '\0';

      // Announce total encoded length; host replies with any byte to start
      // and after every chunk (ACK handshake).
      const size_t CHUNK = 256;
      Serial.print("PHOTO_BEGIN ");
      Serial.print((int)jpg_len);
      Serial.print(" ");
      Serial.println((int)o);   // base64 length
      Serial.flush();

      size_t sent = 0;
      while (sent < o) {
        // wait for host ACK (any byte) before sending the next chunk
        unsigned long t0 = millis();
        while (!Serial.available()) {
          if (millis() - t0 > 5000) { free(enc); Serial.println("PHOTO_ABORT"); Serial.flush(); goto photo_done; }
        }
        Serial.read();

        size_t len = (o - sent < CHUNK) ? (o - sent) : CHUNK;
        Serial.write((const uint8_t *)(enc + sent), len);
        Serial.flush();
        sent += len;
      }
      // final ACK, then end marker
      {
        unsigned long t0 = millis();
        while (!Serial.available() && millis() - t0 < 5000) {}
        if (Serial.available()) Serial.read();
      }
      Serial.println();
      Serial.println("PHOTO_END");
      Serial.flush();
      free(enc);
    photo_done:;
    }

    else {
      Serial.print("Unknown command: ");
      Serial.println(command);
      Serial.flush();

      debugPrint("Use CAPTURE, RUN, TESTMODEL, or PHOTO");
    }
  }
}
