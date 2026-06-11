#include <WiFi.h>
#include <WebServer.h>
#include "esp_sleep.h"
#include "driver/rtc_io.h"
#include "esp_camera.h"
#include "soc/soc.h"
#include "soc/rtc_cntl_reg.h"

const char* ssid = "shacharito";
const char* password = "shachar123456";

WebServer server(80);

#define WAKEUP_PIN GPIO_NUM_13

const unsigned long AWAKE_TIME_MS = 15000;
const unsigned long PHOTO_INTERVAL_MS = 5000;

RTC_DATA_ATTR int bootCount = 0;

unsigned long wakeStartTime = 0;
unsigned long lastPhotoTime = 0;

bool photoAvailable = false;
bool newPhotoAvailable = false;

int photoCounter = 0;
camera_fb_t* photoFrame = NULL;

// AI Thinker ESP32 CAM camera pins
#define PWDN_GPIO_NUM     32
#define RESET_GPIO_NUM    -1
#define XCLK_GPIO_NUM      0
#define SIOD_GPIO_NUM     26
#define SIOC_GPIO_NUM     27

#define Y9_GPIO_NUM       35
#define Y8_GPIO_NUM       34
#define Y7_GPIO_NUM       39
#define Y6_GPIO_NUM       36
#define Y5_GPIO_NUM       21
#define Y4_GPIO_NUM       19
#define Y3_GPIO_NUM       18
#define Y2_GPIO_NUM        5

#define VSYNC_GPIO_NUM    25
#define HREF_GPIO_NUM     23
#define PCLK_GPIO_NUM     22

String getWakeupReasonText() {
  esp_sleep_wakeup_cause_t wakeupReason = esp_sleep_get_wakeup_cause();

  switch (wakeupReason) {
    case ESP_SLEEP_WAKEUP_EXT0:
      return "Wakeup by external trigger on GPIO13";

    case ESP_SLEEP_WAKEUP_TIMER:
      return "Wakeup by timer";

    case ESP_SLEEP_WAKEUP_UNDEFINED:
      return "Power on or reset";

    default:
      return "Other wakeup reason";
  }
}

bool initCamera() {
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

  config.pin_sscb_sda = SIOD_GPIO_NUM;
  config.pin_sscb_scl = SIOC_GPIO_NUM;

  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;

  config.xclk_freq_hz = 20000000;
  config.pixel_format = PIXFORMAT_JPEG;

  if (psramFound()) {
    config.frame_size = FRAMESIZE_VGA;
    config.jpeg_quality = 12;
    config.fb_count = 1;
  } else {
    config.frame_size = FRAMESIZE_QVGA;
    config.jpeg_quality = 15;
    config.fb_count = 1;
  }

  esp_err_t err = esp_camera_init(&config);

  if (err != ESP_OK) {
    Serial.printf("Camera init failed with error 0x%x\n", err);
    return false;
  }

  Serial.println("Camera initialized successfully");
  return true;
}

void capturePhoto() {
  Serial.println("Capturing photo...");

  if (photoFrame != NULL) {
    esp_camera_fb_return(photoFrame);
    photoFrame = NULL;
  }

  delay(200);

  photoFrame = esp_camera_fb_get();

  if (!photoFrame) {
    Serial.println("Camera capture failed");
    photoAvailable = false;
    newPhotoAvailable = false;
    return;
  }

  photoCounter++;
  photoAvailable = true;
  newPhotoAvailable = true;

  Serial.print("Photo captured successfully. Photo number: ");
  Serial.print(photoCounter);
  Serial.print(" | Size: ");
  Serial.print(photoFrame->len);
  Serial.println(" bytes");
}

void handleRoot() {
  Serial.println("Client opened root page");

  String html = "";
  html += "<!DOCTYPE html>";
  html += "<html>";
  html += "<head>";
  html += "<meta charset='UTF-8'>";
  html += "<title>ESP32 Camera Mode</title>";
  html += "</head>";
  html += "<body>";
  html += "<h1>ESP32 Camera Mode</h1>";
  html += "<p>Status: Awake</p>";
  html += "<p>Boot count: ";
  html += bootCount;
  html += "</p>";
  html += "<p>Wakeup reason: ";
  html += getWakeupReasonText();
  html += "</p>";
  html += "<p>Photo counter: ";
  html += photoCounter;
  html += "</p>";

  if (photoAvailable) {
    html += "<p>Photo: Available</p>";
    html += "<img src='/photo.jpg' width='320'>";
  } else {
    html += "<p>Photo: Not available</p>";
  }

  html += "</body>";
  html += "</html>";

  server.send(200, "text/html", html);
}

void handleStatus() {
  Serial.println("Client requested status");

  unsigned long awakeForMs = millis() - wakeStartTime;
  unsigned long remainingMs = 0;

  if (awakeForMs < AWAKE_TIME_MS) {
    remainingMs = AWAKE_TIME_MS - awakeForMs;
  }

  String status = "";
  status += "{";
  status += "\"wifi\":\"connected\",";
  status += "\"device\":\"ESP32-CAM\",";
  status += "\"server\":\"running\",";
  status += "\"power_mode\":\"awake\",";
  status += "\"boot_count\":";
  status += bootCount;
  status += ",";
  status += "\"awake_for_ms\":";
  status += awakeForMs;
  status += ",";
  status += "\"sleep_in_ms\":";
  status += remainingMs;
  status += ",";
  status += "\"photo_counter\":";
  status += photoCounter;
  status += ",";
  status += "\"photo_available\":";
  status += (photoAvailable ? "true" : "false");
  status += ",";
  status += "\"new_photo_available\":";
  status += (newPhotoAvailable ? "true" : "false");
  status += ",";
  status += "\"photo_size_bytes\":";
  status += (photoAvailable && photoFrame != NULL ? photoFrame->len : 0);
  status += ",";
  status += "\"wakeup_reason\":\"";
  status += getWakeupReasonText();
  status += "\"";
  status += "}";

  server.send(200, "application/json", status);
}

void handlePhoto() {
  Serial.println("Client requested photo");

  if (!photoAvailable || photoFrame == NULL) {
    server.send(404, "text/plain", "No photo available");
    return;
  }

  server.send_P(200, "image/jpeg", (const char*)photoFrame->buf, photoFrame->len);

  newPhotoAvailable = false;
}

void goToDeepSleep() {
  Serial.println();
  Serial.println("Preparing to enter Deep Sleep...");
  Serial.println("Wakeup trigger: GPIO13 HIGH");

  if (photoFrame != NULL) {
    esp_camera_fb_return(photoFrame);
    photoFrame = NULL;
  }

  photoAvailable = false;
  newPhotoAvailable = false;

  server.stop();
  WiFi.disconnect(true);
  WiFi.mode(WIFI_OFF);

  rtc_gpio_deinit(WAKEUP_PIN);

  rtc_gpio_init(WAKEUP_PIN);
  rtc_gpio_set_direction(WAKEUP_PIN, RTC_GPIO_MODE_INPUT_ONLY);
  rtc_gpio_pulldown_en(WAKEUP_PIN);
  rtc_gpio_pullup_dis(WAKEUP_PIN);

  esp_sleep_enable_ext0_wakeup(WAKEUP_PIN, 1);

  delay(500);

  Serial.println("Going to Deep Sleep now");
  Serial.flush();

  esp_deep_sleep_start();
}

void setup() {
  WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0);

  Serial.begin(115200);
  delay(1000);

  bootCount++;
  wakeStartTime = millis();
  lastPhotoTime = 0;
  photoCounter = 0;

  Serial.println();
  Serial.println("Starting ESP32 Camera Mode...");
  Serial.print("Boot count: ");
  Serial.println(bootCount);
  Serial.print("Wakeup reason: ");
  Serial.println(getWakeupReasonText());

  pinMode((int)WAKEUP_PIN, INPUT_PULLDOWN);

  WiFi.mode(WIFI_STA);
  WiFi.disconnect(true);
  delay(1000);

  Serial.print("Connecting to WiFi SSID: ");
  Serial.println(ssid);

  WiFi.begin(ssid, password);

  int attempts = 0;

  while (WiFi.status() != WL_CONNECTED && attempts < 40) {
    delay(500);
    Serial.print(".");
    attempts++;
  }

  Serial.println();

  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi connection failed");
    Serial.print("WiFi status code: ");
    Serial.println(WiFi.status());
    Serial.println("Going to Deep Sleep because WiFi failed");
    delay(1000);
    goToDeepSleep();
  }

  Serial.println("WiFi connected successfully");
  Serial.print("IP Address: ");
  Serial.println(WiFi.localIP());
  Serial.print("Signal strength RSSI: ");
  Serial.print(WiFi.RSSI());
  Serial.println(" dBm");

  if (!initCamera()) {
    Serial.println("Camera failed. Going to Deep Sleep.");
    delay(1000);
    goToDeepSleep();
  }

  capturePhoto();
  lastPhotoTime = millis();

  server.on("/", handleRoot);
  server.on("/status", handleStatus);
  server.on("/photo.jpg", handlePhoto);

  server.begin();

  Serial.println("Web Server started");
  Serial.println("Open this address in browser:");
  Serial.print("http://");
  Serial.println(WiFi.localIP());
}

void loop() {
  if (WiFi.status() == WL_CONNECTED) {
    server.handleClient();
  }

  unsigned long now = millis();

  if (now - lastPhotoTime >= PHOTO_INTERVAL_MS) {
    capturePhoto();
    lastPhotoTime = now;
  }

  if (now - wakeStartTime >= AWAKE_TIME_MS) {
    goToDeepSleep();
  }
}