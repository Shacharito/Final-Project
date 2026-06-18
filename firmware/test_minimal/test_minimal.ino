// MINIMAL TEST: Just Serial to find the crashing component

void setup() {
    Serial.begin(460800);
    delay(1000);
    
    Serial.println("\n\n╔════════════════════════════╗");
    Serial.println("║  MINIMAL TEST FIRMWARE     ║");
    Serial.println("╚════════════════════════════╝");
    
    Serial.println("[BOOT] Serial OK");
    
    Serial.println("[BOOT] About to test WiFi.begin()...");
    WiFi.begin("Amit_wifi", "Amit1998");
    Serial.println("[BOOT] WiFi.begin() completed - no crash!");
    
    Serial.println("[BOOT] All tests passed!");
}

void loop() {
    delay(1000);
    Serial.println("[LOOP] Device running");
}
