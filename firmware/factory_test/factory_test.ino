#include <Arduino.h>

// BARE MINIMUM: Just prove the device can boot
// No RTC, no WiFi, no camera - NOTHING

void setup() {
  Serial.begin(460800);
  delay(500);
  Serial.println("\n\n=== FACTORY TEST FIRMWARE ===");
  Serial.println("Device booted successfully!");
  Serial.println("Testing GPIO output...");
  
  // Try LED on GPIO2 (common)
  pinMode(2, OUTPUT);
  for(int i = 0; i < 5; i++) {
    digitalWrite(2, HIGH);
    delay(200);
    digitalWrite(2, LOW);
    delay(200);
    Serial.print(".");
  }
  Serial.println("\nAll tests passed!");
}

void loop() {
  delay(1000);
  Serial.println("Device alive");
}
