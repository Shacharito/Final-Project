#include <WiFi.h>
#include <WebServer.h>
#include "esp_sleep.h"
#include "driver/rtc_io.h"

const char* ssid = "shacharito";
const char* password = "shachar123456";

WebServer server(80);

#define WAKEUP_PIN GPIO_NUM_13

const unsigned long AWAKE_TIME_MS = 20000;

RTC_DATA_ATTR int bootCount = 0;

unsigned long wakeStartTime = 0;

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

void handleRoot() {
  Serial.println("Client opened root page");

  String html = "";
  html += "<!DOCTYPE html>";
  html += "<html>";
  html += "<head>";
  html += "<meta charset='UTF-8'>";
  html += "<title>ESP32 Sleep Mode Test</title>";
  html += "</head>";
  html += "<body>";
  html += "<h1>ESP32 Sleep Mode Web Server</h1>";
  html += "<p>Status: Awake</p>";
  html += "<p>Boot count: ";
  html += bootCount;
  html += "</p>";
  html += "<p>Wakeup reason: ";
  html += getWakeupReasonText();
  html += "</p>";
  html += "<p>The ESP32 will go back to Deep Sleep after 20 seconds.</p>";
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
  status += "\"wakeup_reason\":\"";
  status += getWakeupReasonText();
  status += "\"";
  status += "}";

  server.send(200, "application/json", status);
}

void goToDeepSleep() {
  Serial.println();
  Serial.println("Preparing to enter Deep Sleep...");
  Serial.println("Wakeup trigger: GPIO13 HIGH");

  server.stop();
  WiFi.disconnect(true);
  WiFi.mode(WIFI_OFF);

  rtc_gpio_deinit(WAKEUP_PIN);
  pinMode((int)WAKEUP_PIN, INPUT_PULLDOWN);

  esp_sleep_enable_ext0_wakeup(WAKEUP_PIN, 1);

  delay(500);

  Serial.println("Going to Deep Sleep now");
  Serial.flush();

  esp_deep_sleep_start();
}

void setup() {
  Serial.begin(115200);
  delay(1000);

  bootCount++;
  wakeStartTime = millis();

  Serial.println();
  Serial.println("Starting ESP32 Sleep Mode Web Server...");
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

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("WiFi connected successfully");
    Serial.print("IP Address: ");
    Serial.println(WiFi.localIP());
    Serial.print("Signal strength RSSI: ");
    Serial.print(WiFi.RSSI());
    Serial.println(" dBm");

    server.on("/", handleRoot);
    server.on("/status", handleStatus);

    server.begin();
    Serial.println("Web Server started");
    Serial.println("Open this address in browser:");
    Serial.print("http://");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("WiFi connection failed");
    Serial.print("WiFi status code: ");
    Serial.println(WiFi.status());
    Serial.println("Going to Deep Sleep because WiFi failed");
    delay(1000);
    goToDeepSleep();
  }
}

void loop() {
  if (WiFi.status() == WL_CONNECTED) {
    server.handleClient();
  }

  if (millis() - wakeStartTime >= AWAKE_TIME_MS) {
    goToDeepSleep();
  }
}