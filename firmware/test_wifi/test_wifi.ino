#include <Arduino.h>
#include <WiFi.h>

// Test: WiFi only - no WebServer, no RTC, no camera

// Fallback WiFi credentials
const char* SSID_FALLBACK = "Amit_wifi";
const char* PASSWORD_FALLBACK = "Amit1998";

void setup() {
  Serial.begin(460800);
  delay(500);
  Serial.println("\n\n=== WiFi Test Firmware ===");
  
  Serial.print("Connecting to WiFi: ");
  Serial.println(SSID_FALLBACK);
  
  WiFi.mode(WIFI_STA);
  WiFi.begin(SSID_FALLBACK, PASSWORD_FALLBACK);
  
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 20) {
    delay(500);
    Serial.print(".");
    attempts++;
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nWiFi Connected!");
    Serial.print("IP: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("\nWiFi Connection Failed");
  }
  
  Serial.println("Setup complete");
}

void loop() {
  delay(1000);
  Serial.print("Status: ");
  Serial.println(WiFi.status() == WL_CONNECTED ? "Connected" : "Disconnected");
}
