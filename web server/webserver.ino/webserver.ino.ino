#include <WiFi.h>
#include <WebServer.h>

const char* ssid = "iPhone de Shachar";
const char* password = "shachar123456";

WebServer server(80);

void handleRoot() {
  String html = "";
  html += "<!DOCTYPE html>";
  html += "<html>";
  html += "<head>";
  html += "<meta charset='UTF-8'>";
  html += "<title>ESP32 Web Server</title>";
  html += "</head>";
  html += "<body>";
  html += "<h1>ESP32 is connected</h1>";
  html += "<p>Web Server is working successfully.</p>";
  html += "<p>Final Project ESP32 Controller</p>";
  html += "</body>";
  html += "</html>";

  server.send(200, "text/html", html);
}

void handleStatus() {
  String status = "";
  status += "{";
  status += "\"wifi\":\"connected\",";
  status += "\"device\":\"ESP32\",";
  status += "\"server\":\"running\"";
  status += "}";

  server.send(200, "application/json", status);
}

void setup() {
  Serial.begin(115200);
  delay(1000);

  Serial.println();
  Serial.println("Connecting to WiFi...");

  WiFi.mode(WIFI_STA);
  WiFi.disconnect(true);
  delay(1000);

  WiFi.begin(ssid, password);

  int attempts = 0;

  while (WiFi.status() != WL_CONNECTED && attempts < 30) {
    delay(500);
    Serial.print(".");
    attempts++;
  }

  Serial.println();

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("WiFi connected successfully");
    Serial.print("ESP32 IP Address: ");
    Serial.println(WiFi.localIP());

    server.on("/", handleRoot);
    server.on("/status", handleStatus);

    server.begin();
    Serial.println("Web Server started");
  } else {
    Serial.println("WiFi connection failed");
    Serial.print("WiFi status code: ");
    Serial.println(WiFi.status());
    Serial.println("Web Server was not started because WiFi is not connected");
  }
}
void loop() {
  server.handleClient();
}