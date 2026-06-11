#include "esp_heap_caps.h"
#include <esp32_person_classifier_inferencing.h>
#include "esp_camera.h"

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

void printMemoryStatus(const char *stage) {
  Serial.println();
  Serial.print("===== MEMORY STATUS: ");
  Serial.print(stage);
  Serial.println(" =====");

  Serial.print("Free internal heap: ");
  Serial.println(heap_caps_get_free_size(MALLOC_CAP_INTERNAL));

  Serial.print("Largest internal block: ");
  Serial.println(heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL));

  Serial.print("Free PSRAM: ");
  Serial.println(heap_caps_get_free_size(MALLOC_CAP_SPIRAM));

  Serial.print("Largest PSRAM block: ");
  Serial.println(heap_caps_get_largest_free_block(MALLOC_CAP_SPIRAM));

  Serial.println("================================");
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

  Serial.println("First 5 pixels after capture conversion:");
  for (int i = 0; i < 5; i++) {
    int idx = i * 3;

    Serial.print("Pixel ");
    Serial.print(i);
    Serial.print(": R=");
    Serial.print(snapshot_buf[idx + 0]);
    Serial.print(", G=");
    Serial.print(snapshot_buf[idx + 1]);
    Serial.print(", B=");
    Serial.println(snapshot_buf[idx + 2]);
  }
  Serial.flush();

  esp_camera_fb_return(fb);

  debugPrint("CAPTURE E: frame returned");

  return true;
}

int get_data(size_t offset, size_t length, float *out_ptr) {
  static int call_count = 0;

  if (!snapshot_buf) {
    Serial.println("ERROR in get_data: snapshot_buf is NULL");
    Serial.flush();
    return -1;
  }

  if ((offset + length) > EI_CLASSIFIER_RAW_SAMPLE_COUNT) {
    Serial.println("ERROR in get_data: requested data out of range");
    Serial.print("offset: ");
    Serial.println(offset);
    Serial.print("length: ");
    Serial.println(length);
    Serial.print("offset + length: ");
    Serial.println(offset + length);
    Serial.print("EI_CLASSIFIER_RAW_SAMPLE_COUNT: ");
    Serial.println(EI_CLASSIFIER_RAW_SAMPLE_COUNT);
    Serial.flush();
    return -1;
  }

  if (call_count < 10) {
    Serial.println();
    Serial.println("===== get_data CALLED =====");
    Serial.print("call_count: ");
    Serial.println(call_count);

    Serial.print("offset: ");
    Serial.println(offset);

    Serial.print("length: ");
    Serial.println(length);

    Serial.print("offset + length: ");
    Serial.println(offset + length);

    Serial.print("EI_CLASSIFIER_RAW_SAMPLE_COUNT: ");
    Serial.println(EI_CLASSIFIER_RAW_SAMPLE_COUNT);

    Serial.print("IMG_WIDTH * IMG_HEIGHT: ");
    Serial.println(IMG_WIDTH * IMG_HEIGHT);

    Serial.println("=================================");
    Serial.flush();
  }

  size_t pixel_ix = offset * 3;

  for (size_t i = 0; i < length; i++) {
    uint8_t r = snapshot_buf[pixel_ix + 0];
    uint8_t g = snapshot_buf[pixel_ix + 1];
    uint8_t b = snapshot_buf[pixel_ix + 2];

    out_ptr[i] = (r << 16) + (g << 8) + b;

    if (call_count < 10 && i < 5) {
      Serial.print("get_data sample pixel ");
      Serial.print(i);
      Serial.print(": R=");
      Serial.print(r);
      Serial.print(", G=");
      Serial.print(g);
      Serial.print(", B=");
      Serial.print(b);
      Serial.print(", packed=");
      Serial.println(out_ptr[i]);
    }

    pixel_ix += 3;
  }

  if (call_count < 10) {
    Serial.println("get_data finished successfully");
    Serial.flush();
  }

  call_count++;

  return 0;
}

int get_dummy_data(size_t offset, size_t length, float *out_ptr) {
  static int call_count = 0;

  if ((offset + length) > EI_CLASSIFIER_RAW_SAMPLE_COUNT) {
    Serial.println("ERROR in get_dummy_data: requested data out of range");
    Serial.print("offset: ");
    Serial.println(offset);
    Serial.print("length: ");
    Serial.println(length);
    Serial.print("offset + length: ");
    Serial.println(offset + length);
    Serial.print("EI_CLASSIFIER_RAW_SAMPLE_COUNT: ");
    Serial.println(EI_CLASSIFIER_RAW_SAMPLE_COUNT);
    Serial.flush();
    return -1;
  }

  if (call_count < 10) {
    Serial.println();
    Serial.println("===== get_dummy_data CALLED =====");
    Serial.print("call_count: ");
    Serial.println(call_count);

    Serial.print("offset: ");
    Serial.println(offset);

    Serial.print("length: ");
    Serial.println(length);

    Serial.print("offset + length: ");
    Serial.println(offset + length);

    Serial.print("EI_CLASSIFIER_RAW_SAMPLE_COUNT: ");
    Serial.println(EI_CLASSIFIER_RAW_SAMPLE_COUNT);

    Serial.println("Returning black pixels: 0x000000");
    Serial.println("=================================");
    Serial.flush();
  }

  for (size_t i = 0; i < length; i++) {
    out_ptr[i] = 0x000000;
  }

  if (call_count < 10) {
    Serial.println("get_dummy_data finished successfully");
    Serial.flush();
  }

  call_count++;

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
  Serial.println();
  Serial.println("===== MODEL INFO =====");

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

  Serial.print("IMG_WIDTH * IMG_HEIGHT: ");
  Serial.println(IMG_WIDTH * IMG_HEIGHT);

  Serial.print("IMG_WIDTH * IMG_HEIGHT * 3: ");
  Serial.println(IMG_WIDTH * IMG_HEIGHT * 3);

  Serial.println("======================");
  Serial.flush();
}

void setup() {
  Serial.begin(115200);
  delay(3000);

  Serial.println();
  Serial.println("=== ESP32 S3 Human Detection System DEBUG DIAGNOSTIC ===");
  Serial.flush();

  printModelInfo();
  printMemoryStatus("AFTER printModelInfo / BEFORE PSRAM check");

  if (psramFound()) {
    debugPrint("PSRAM found");
  } else {
    debugPrint("PSRAM not found");
  }

  debugPrint("SETUP A: before snapshot buffer allocation");

  size_t snapshot_size = IMG_WIDTH * IMG_HEIGHT * 3;

  Serial.print("Snapshot buffer size: ");
  Serial.println(snapshot_size);
  Serial.flush();

  printMemoryStatus("BEFORE snapshot buffer allocation");

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
  printMemoryStatus("AFTER snapshot buffer allocation");

  debugPrint("SETUP C: before initCamera");

  initCamera();

  debugPrint("SETUP D: after initCamera");
  printMemoryStatus("AFTER camera init");

  debugPrint("System ready");

  Serial.println();
  Serial.println("Send CAPTURE to test camera only");
  Serial.println("Send RUN to capture and run inference");
  Serial.println("Send TESTMODEL to test model with dummy black image");
  Serial.flush();
}

void loop() {
  if (Serial.available()) {
    String command = Serial.readStringUntil('\n');
    command.trim();

    if (command == "CAPTURE") {
      debugPrint("COMMAND RECEIVED: CAPTURE");

      printMemoryStatus("BEFORE CAPTURE");

      debugPrint("TEST 1: before captureImageForModel");

      bool ok = captureImageForModel();

      debugPrint("TEST 2: after captureImageForModel");

      printMemoryStatus("AFTER CAPTURE");

      if (!ok) {
        debugPrint("Image capture failed");
        return;
      }

      debugPrint("TEST 3: image capture and conversion succeeded");
    }

    else if (command == "RUN") {
      debugPrint("COMMAND RECEIVED: RUN");

      printMemoryStatus("BEFORE RUN capture");

      debugPrint("STEP 1: before captureImageForModel");

      bool ok = captureImageForModel();

      debugPrint("STEP 2: after captureImageForModel");

      printMemoryStatus("AFTER RUN capture / BEFORE signal");

      if (!ok) {
        debugPrint("Image capture failed");
        return;
      }

      signal_t signal;
      signal.total_length = EI_CLASSIFIER_RAW_SAMPLE_COUNT;
      signal.get_data = &get_data;

      Serial.println();
      Serial.println("===== RUN SIGNAL INFO =====");

      Serial.print("Signal total length used: ");
      Serial.println(signal.total_length);

      Serial.print("Expected raw sample count: ");
      Serial.println(EI_CLASSIFIER_RAW_SAMPLE_COUNT);

      Serial.print("Expected NN input frame size: ");
      Serial.println(EI_CLASSIFIER_NN_INPUT_FRAME_SIZE);

      Serial.print("IMG_WIDTH * IMG_HEIGHT: ");
      Serial.println(IMG_WIDTH * IMG_HEIGHT);

      Serial.print("IMG_WIDTH * IMG_HEIGHT * 3: ");
      Serial.println(IMG_WIDTH * IMG_HEIGHT * 3);

      Serial.println("===========================");
      Serial.flush();

      ei_impulse_result_t result = {0};

      debugPrint("STEP 3: before run_classifier");
      printMemoryStatus("BEFORE RUN run_classifier");

      EI_IMPULSE_ERROR res = run_classifier(&signal, &result, true);

      printMemoryStatus("AFTER RUN run_classifier");
      debugPrint("STEP 4: after run_classifier");

      if (res != EI_IMPULSE_OK) {
        Serial.println("Classifier failed");
        printEiError(res);
        return;
      }

      debugPrint("STEP 5: before printResults");

      printResults(result);

      debugPrint("STEP 6: after printResults");
    }

    else if (command == "TESTMODEL") {
      debugPrint("COMMAND RECEIVED: TESTMODEL");
      debugPrint("Testing model with dummy black image");

      printMemoryStatus("BEFORE TESTMODEL signal");

      signal_t signal;
      signal.total_length = EI_CLASSIFIER_RAW_SAMPLE_COUNT;
      signal.get_data = &get_dummy_data;

      Serial.println();
      Serial.println("===== TESTMODEL SIGNAL INFO =====");

      Serial.print("Dummy signal length: ");
      Serial.println(signal.total_length);

      Serial.print("Expected raw sample count: ");
      Serial.println(EI_CLASSIFIER_RAW_SAMPLE_COUNT);

      Serial.print("Expected NN input frame size: ");
      Serial.println(EI_CLASSIFIER_NN_INPUT_FRAME_SIZE);

      Serial.print("IMG_WIDTH * IMG_HEIGHT: ");
      Serial.println(IMG_WIDTH * IMG_HEIGHT);

      Serial.print("IMG_WIDTH * IMG_HEIGHT * 3: ");
      Serial.println(IMG_WIDTH * IMG_HEIGHT * 3);

      Serial.println("===============================");
      Serial.flush();

      ei_impulse_result_t result = {0};

      debugPrint("TESTMODEL 1: before run_classifier");
      printMemoryStatus("BEFORE TESTMODEL run_classifier");

      EI_IMPULSE_ERROR res = run_classifier(&signal, &result, true);

      printMemoryStatus("AFTER TESTMODEL run_classifier");
      debugPrint("TESTMODEL 2: after run_classifier");

      if (res != EI_IMPULSE_OK) {
        Serial.println("Dummy model test failed");
        printEiError(res);
        return;
      }

      Serial.println("Dummy model test succeeded");
      printResults(result);
    }

    else {
      Serial.print("Unknown command: ");
      Serial.println(command);
      Serial.flush();

      debugPrint("Use CAPTURE, RUN, or TESTMODEL");
    }
  }
}