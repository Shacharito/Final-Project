#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>

// Test: WiFi + WebServer (order matters!)

const char* SSID_FALLBACK = "Amit_wifi";
const char* PASSWORD_FALLBACK = "Amit1998";

WebServer server(80);

void handleRoot() {
  server.send(200, "application/json", "{\"status\":\"ok\"}");
}

void setup() {
  Serial.begin(460800);
  delay(500);
  Serial.println("\n\n=== WiFi + WebServer Test ===");
  
  Serial.println("1. Initializing WiFi...");
  WiFi.mode(WIFI_STA);
  WiFi.begin(SSID_FALLBACK, PASSWORD_FALLBACK);
  
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 20) {
    delay(500);
    Serial.print(".");
    attempts++;
  }
  Serial.println();
  
  if (WiFi.status() == WL_CONNECTED) {
    Serial.print("WiFi connected! IP: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("WiFi connection failed - continuing anyway");
  }
  
  Serial.println("2. Setting up WebServer...");
  server.on("/", handleRoot);
  server.begin();
  Serial.println("WebServer started!");
  
  Serial.println("✅ Setup complete - both WiFi and WebServer working");
}

void loop() {
  server.handleClient();
  delay(10);
}
