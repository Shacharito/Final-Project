#include <Arduino.h>
#include <WebServer.h>

// Test: WebServer only - no WiFi, no RTC, no camera

WebServer server(80);

void handleRoot() {
  server.send(200, "application/json", "{\"status\":\"ok\"}");
}

void setup() {
  Serial.begin(460800);
  delay(500);
  Serial.println("\n\n=== WebServer Test Firmware ===");
  
  Serial.println("Setting up WebServer...");
  
  server.on("/", handleRoot);
  server.begin();
  
  Serial.println("WebServer started on port 80");
  Serial.println("Setup complete");
}

void loop() {
  server.handleClient();
  delay(100);
  Serial.println("Loop running");
}
