#include <WiFi.h>
#include <WebServer.h>
const char* ssid = "shacharito";
const char* password = "shachar123456";
WebServer server(80);
void handleRoot() {
  Serial.println("Client opened root page");
  String html = "";
  html += "<!DOCTYPE html>";
  html += "<html>";
  html += "<head>";
  html += "<meta charset='UTF-8'>";
  html += "<title>ESP32 Web Server</title>";
  html += "</head>";
  html += "<body>";
  html += "<h1>ESP32 Web Server is working</h1>";
  html += "<p>WiFi connected successfully.</p>";
  html += "<p>Final Project ESP32 Controller</p>";
  html += "<p>Status: Online</p>";
  html += "</body>";
  html += "</html>";
  server.send(200, "text/html", html);
}
void handleStatus() {
  Serial.println("Client requested status");

  String detectionStatus;

  if ((millis() / 5000) % 2 == 0) {
    detectionStatus = "No detection";
  } else {
    detectionStatus = "Detection";
  }

  String status = "";
  status += "{";
  status += "\"wifi\":\"connected\",";
  status += "\"device\":\"ESP32\",";
  status += "\"server\":\"running\",";
  status += "\"detection\":\"";
  status += detectionStatus;
  status += "\"";
  status += "}";

  server.send(200, "application/json", status);
}
void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println();
  Serial.println("Starting ESP32 Web Server...");
  Serial.print("SSID: ");
  Serial.println(ssid);
  WiFi.mode(WIFI_STA);
  WiFi.disconnect(true);
  delay(1000);
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
    Serial.println("Web Server was not started");
  }
}
void loop() {
  if (WiFi.status() == WL_CONNECTED) {
    server.handleClient();
  }
}