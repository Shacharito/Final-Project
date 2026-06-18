#include "config.h"
#include <WiFi.h>
#include <WebServer.h>
#include "soc/soc.h"
#include "soc/rtc_cntl_reg.h"

// Minimal firmware to test WITHOUT camera initialization
// This helps us identify if camera is the problem

WebServer server(80);
RTC_DATA_ATTR int bootCount = 0;

void setup() {
  WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0); // Disable brownout detector
  Serial.begin(460800);
  delay(1000);
  
  Serial.println("\n\n=== DIAGNOSTIC FIRMWARE (NO CAMERA) ===");
  bootCount++;
  Serial.printf("Boot count: %d\n", bootCount);
  
  // Setup web server endpoints
  server.on("/health", HTTP_GET, []() {
    String response = "{\"ok\":true,\"boot_count\":" + String(bootCount) + "}";
    server.send(200, "application/json", response);
  });
  
  server.on("/config", HTTP_GET, []() {
    String response = "{";
    response += "\"ssid\":\"" + String(WIFI_SSID_FALLBACK) + "\",";
    response += "\"boot_count\":" + String(bootCount);
    response += "}";
    server.send(200, "application/json", response);
  });
  
  server.begin();
  Serial.println("Web server started on port 80");
  
  // WiFi connection
  Serial.printf("Connecting to WiFi: %s\n", WIFI_SSID_FALLBACK);
  WiFi.begin(WIFI_SSID_FALLBACK, WIFI_PASSWORD_FALLBACK);
  
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 20) {
    delay(500);
    Serial.print(".");
    attempts++;
  }
  
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("\nWiFi FAILED - Restarting");
    delay(2000);
    ESP.restart();
  }
  
  Serial.printf("\n✓ WiFi connected!\n");
  Serial.printf("✓ IP Address: %s\n", WiFi.localIP().toString().c_str());
  Serial.println("\nAvailable endpoints:");
  Serial.println("  GET /health  - Check device status");
  Serial.println("  GET /config  - Get configuration");
  Serial.println("\n=== Ready ===\n");
}

void loop() {
  server.handleClient();
  delay(10);
}
