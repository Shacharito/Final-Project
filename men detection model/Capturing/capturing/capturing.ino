#include "esp_camera.h"
#include <HardwareSerial.h>

HardwareSerial DebugUART(0);

// =========================
// Camera pins
// אם אצלך יש pinout אחר, החלף רק את החלק הזה לפי הקוד הקודם שעבד
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

#define UART_BAUD_RATE 115200

void printBoth(String msg) {
  Serial.println(msg);
  DebugUART.println(msg);
}

void setupCamera() {
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
  config.pixel_format = PIXFORMAT_JPEG;

  if (psramFound()) {
    config.frame_size = FRAMESIZE_SVGA;
    config.jpeg_quality = 8;
    config.fb_count = 2;
    config.fb_location = CAMERA_FB_IN_PSRAM;
  } else {
    config.frame_size = FRAMESIZE_VGA;
    config.jpeg_quality = 10;
    config.fb_count = 1;
    config.fb_location = CAMERA_FB_IN_DRAM;
  }

  config.grab_mode = CAMERA_GRAB_LATEST;

  esp_err_t err = esp_camera_init(&config);

  if (err != ESP_OK) {
    DebugUART.print("Camera init failed. Error code: ");
    DebugUART.println(err);
    while (true) {
      delay(1000);
    }
  }

  sensor_t *s = esp_camera_sensor_get();

  s->set_brightness(s, 0);
  s->set_contrast(s, 1);
  s->set_saturation(s, 0);
  s->set_sharpness(s, 1);
  s->set_denoise(s, 1);
  s->set_gainceiling(s, GAINCEILING_4X);
  s->set_quality(s, 8);

  DebugUART.println("Camera ready");
}

void captureAndSendImage() {
  DebugUART.println("CAPTURE command received");
  DebugUART.println("Taking picture...");

  camera_fb_t *fb = esp_camera_fb_get();

  if (!fb) {
    DebugUART.println("Capture failed");
    return;
  }

  DebugUART.print("Picture taken. Size: ");
  DebugUART.println(fb->len);

  DebugUART.println("IMG_BEGIN");

  uint32_t imageSize = fb->len;
  DebugUART.write((uint8_t *)&imageSize, 4);
  DebugUART.write(fb->buf, fb->len);

  DebugUART.println();
  DebugUART.println("IMG_END");

  esp_camera_fb_return(fb);

  DebugUART.println("Image sent");
}

void setup() {
  Serial.begin(UART_BAUD_RATE);

  DebugUART.begin(UART_BAUD_RATE, SERIAL_8N1, 44, 43);

  delay(3000);

  DebugUART.println();
  DebugUART.println("ESP32 S3 CAM started");
  DebugUART.println("UART communication ready");

  if (psramFound()) {
    DebugUART.println("PSRAM found");
  } else {
    DebugUART.println("PSRAM not found");
  }

  setupCamera();

  DebugUART.println("Ready. Send CAPTURE to take image.");
}

void loop() {
  if (DebugUART.available()) {
    String command = DebugUART.readStringUntil('\n');
    command.trim();

    DebugUART.print("I received: ");
    DebugUART.println(command);

    if (command == "CAPTURE") {
      captureAndSendImage();
    }
  }
}