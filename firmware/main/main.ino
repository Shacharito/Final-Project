#include "config.h"
#include <WiFi.h>
#include <WebServer.h>
#include "esp_sleep.h"
#include "driver/rtc_io.h"
#include "esp_camera.h"
#include "soc/soc.h"
#include "soc/rtc_cntl_reg.h"
#include "esp_heap_caps.h"

// ==================================
// MEMORY CALLBACKS (must be BEFORE Edge Impulse include!)
// ==================================

void *ei_malloc(size_t size) {
    void *ptr = heap_caps_malloc(size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    return ptr;
}

void *ei_calloc(size_t nitems, size_t size) {
    return heap_caps_calloc(nitems, size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
}

void ei_free(void *ptr) {
    free(ptr);
}

void ei_printf(const char *format, ...) {
    char buf[256];
    va_list args;
    va_start(args, format);
    vsnprintf(buf, sizeof(buf), format, args);
    va_end(args);
    Serial.print(buf);
    Serial.flush();
}

// NOW include Edge Impulse AFTER memory callbacks are defined
#include <esp32_person_classifier_inferencing.h>

// ==================================
// SECTION 1: CONFIGURATION & STATE
// ==================================

// User Configuration (managed by Python client)
char wifi_ssid[32] = "";
char wifi_password[64] = "";
unsigned long sleep_duration_sec = 60;
float person_threshold = 0.7;

// Device State Tracking - tracks what's working and what failed
typedef struct {
  bool wifi_connected = false;
  bool camera_initialized = false;
  bool model_loaded = false;
  bool pir_available = false;
  bool hlk_available = false;
  uint32_t camera_init_error = 0;
  uint32_t last_inference_ms = 0;
  uint32_t last_capture_error = 0;
  unsigned long uptime_seconds = 0;
  uint8_t pir_last_state = 0;      // Last PIR reading
  uint32_t hlk_last_distance_mm = 0; // Last HLK distance in mm
  uint8_t hlk_detection_state = 0;   // 0=no detection, 1=detection
} DeviceState;

// Global Objects
WebServer server(80);
RTC_DATA_ATTR int bootCount = 0;
DeviceState device_state;

// Buffers
static uint8_t *jpg_buf = nullptr;
static size_t   jpg_len = 0;
static uint8_t *snapshot_buf = nullptr;
static unsigned long boot_time_ms = 0;

// ==================================
// SECTION 2: UTILITY FUNCTIONS
// ================================== 

// Get device uptime
unsigned long getUptimeSeconds() {
    return (millis() - boot_time_ms) / 1000;
}

// Get human-readable error message
String getErrorMessage(uint32_t error_code) {
    switch (error_code) {
        case 0x20001: return "Camera I/C2C error";
        case 0x20002: return "Camera power down pin error";
        case 0x20003: return "Camera reset pin error";
        case 0x20004: return "Camera XCLK error";
        case 0x20005: return "Camera DMA error";
        default: return "Unknown error (0x" + String(error_code, HEX) + ")";
    }
}

// Callback to get data for inference (safely)
int get_data(size_t offset, size_t length, float *out_ptr) {
    if (!snapshot_buf) {
        Serial.println("[ERROR] snapshot_buf is NULL");
        return -1;
    }
    size_t pixel_ix = offset * 3;
    for (size_t i = 0; i < length; i++) {
        uint8_t r = snapshot_buf[pixel_ix + 0];
        uint8_t g = snapshot_buf[pixel_ix + 1];
        uint8_t b = snapshot_buf[pixel_ix + 2];
        out_ptr[i] = (r << 16) + (g << 8) + b;
        pixel_ix += 3;
    }
    return 0;
}

// ==================================
// SECTION 2B: SENSOR FUNCTIONS
// ==================================

// Initialize GROVE-PIR sensor (simple GPIO)
void initGROVEPIR() {
    Serial.println("[PIR:0] Initializing GROVE PIR on GPIO21...");
    Serial.printf("[PIR:1] GROVE_PIR_PIN = %d\n", GROVE_PIR_PIN);
    pinMode(GROVE_PIR_PIN, INPUT);
    Serial.println("[PIR:2] pinMode() called");
    device_state.pir_available = true;
    Serial.println("[PIR:3] ✓ GROVE PIR ready");
}

// Initialize HLK-LD2451 millimeter-wave radar (UART)
void initHLK_LD2451() {
    Serial.println("[HLK:0] Initializing HLK-LD2451 on UART2...");
    Serial.printf("[HLK:1] TX pin: GPIO%d\n", HLK_LD2451_TX_PIN);
    Serial.printf("[HLK:2] RX pin: GPIO%d\n", HLK_LD2451_RX_PIN);
    Serial.println("[HLK:3] Configuring UART2 at 115200 baud");
    
    // Configure UART2 for HLK-LD2451 communication
    // Parameters: uart_num, baud_rate, tx_pin, rx_pin
    
    // Note: UART2 begin will be called from main setup
    device_state.hlk_available = true;
    Serial.println("[HLK:4] ✓ HLK-LD2451 ready");
}

// Initialize all sensors (gracefully handle missing sensors)
void initSensors() {
    Serial.println("\n[SENSORS:0] Initializing GROVE-PIR and HLK-LD2451...");
    
    // Initialize GROVE-PIR sensor
    Serial.println("[SENSORS:1] Attempting GROVE-PIR init...");
    try {
        initGROVEPIR();
        Serial.println("[SENSORS:2] GROVE-PIR init successful");
    } catch (...) {
        Serial.println("[SENSORS:2] ❌ GROVE-PIR init failed - continuing without");
        device_state.pir_available = false;
    }
    
    // Initialize HLK-LD2451 sensor
    Serial.println("[SENSORS:3] Attempting HLK-LD2451 init...");
    try {
        initHLK_LD2451();
        Serial.println("[SENSORS:4] HLK-LD2451 init successful");
    } catch (...) {
        Serial.println("[SENSORS:4] ❌ HLK-LD2451 init failed - continuing without");
        device_state.hlk_available = false;
    }
    
    Serial.printf("[SENSORS:5] Status: PIR=%s, HLK=%s\n",
        device_state.pir_available ? "✓" : "✗",
        device_state.hlk_available ? "✓" : "✗");
}

// Read GROVE-PIR sensor (simple GPIO read)
bool readGROVEPIR() {
    if (!device_state.pir_available) {
        Serial.println("[PIR:READ] PIR not available - returning false");
        return false; // Sensor not available
    }
    
    Serial.println("[PIR:READ] Reading GPIO21...");
    uint8_t pir_state = digitalRead(GROVE_PIR_PIN);
    Serial.printf("[PIR:READ] GPIO21 = %d\n", pir_state);
    device_state.pir_last_state = pir_state;
    
    if (pir_state == HIGH) {
        Serial.println("[PIR] ✓ Motion detected!");
        return true;
    }
    return false;
}

// Read HLK-LD2451 sensor via UART
// NOTE: Full implementation depends on your sensor's command protocol
// This is a placeholder that monitors for data on UART2
bool readHLK_LD2451() {
    if (!device_state.hlk_available) {
        Serial.println("[HLK:READ] HLK not available - returning false");
        return false; // Sensor not available
    }
    
    Serial.println("[HLK:READ] Checking Serial2 for data...");
    
    // The HLK-LD2451 sends detection data periodically via UART2
    // This function would need to:
    // 1. Read data from UART2 serial buffer
    // 2. Parse the detection frames
    // 3. Extract distance and detection state
    // 4. Return true if object detected
    
    // Placeholder: Check if data available on UART2
    if (Serial2.available()) {
        Serial.println("[HLK:READ] Data available on Serial2, reading...");
        String hlk_data = Serial2.readStringUntil('\n');
        Serial.printf("[HLK:READ] Received: %s\n", hlk_data.c_str());
        
        // Example: If response contains "Distance" or detection indicator
        if (hlk_data.indexOf("Distance") >= 0 || hlk_data.indexOf("target") >= 0) {
            device_state.hlk_detection_state = 1;
            Serial.println("[HLK:READ] ✓ Target detected!");
            return true;
        }
    } else {
        Serial.println("[HLK:READ] No data on Serial2");
    }
    
    return false;
}

// Check if any sensor triggered detection
bool shouldRunInference() {
    Serial.println("[TRIGGER:0] Checking sensors for trigger...");
    
    // Option 1: GROVE-PIR motion detected
    Serial.println("[TRIGGER:1] Checking GROVE-PIR...");
    if (readGROVEPIR()) {
        Serial.println("[TRIGGER:2] ✓ GROVE-PIR detected motion");
        return true;
    }
    Serial.println("[TRIGGER:2] GROVE-PIR: no motion");
    
    // Option 2: HLK-LD2451 detected target
    Serial.println("[TRIGGER:3] Checking HLK-LD2451...");
    if (readHLK_LD2451()) {
        Serial.println("[TRIGGER:4] ✓ HLK-LD2451 detected target");
        return true;
    }
    Serial.println("[TRIGGER:4] HLK-LD2451: no target");
    
    Serial.println("[TRIGGER:5] No sensor triggered");
    return false; // No trigger
}

// ==================================
// SECTION 3: POWER MANAGEMENT
// ==================================

void goToDeepSleep(unsigned long duration_sec) {
    Serial.printf("\n[SLEEP] Preparing deep sleep for %lu seconds\n", duration_sec);
    Serial.println("[SLEEP] Wakeup triggers: GPIO13 HIGH (sensor) or timer");
    
    // Cleanup
    if (jpg_buf) { free(jpg_buf); jpg_buf = nullptr; jpg_len = 0; }
    if (snapshot_buf) { free(snapshot_buf); snapshot_buf = nullptr; }
    
    // Shutdown services
    server.stop();
    WiFi.disconnect(true);
    WiFi.mode(WIFI_OFF);
    
    // Configure GPIO wakeup (for external sensor)
    rtc_gpio_init(WAKEUP_PIN);
    rtc_gpio_set_direction(WAKEUP_PIN, RTC_GPIO_MODE_INPUT_ONLY);
    rtc_gpio_pulldown_en(WAKEUP_PIN);
    rtc_gpio_pullup_dis(WAKEUP_PIN);
    esp_sleep_enable_ext0_wakeup(WAKEUP_PIN, 1);
    
    // Configure timer wakeup
    if (duration_sec > 0) {
        esp_sleep_enable_timer_wakeup(duration_sec * 1000000);
    }
    
    delay(500);
    Serial.println("[SLEEP] Entering deep sleep now...");
    Serial.flush();
    esp_deep_sleep_start();
}

String getWakeupReasonText() {
    esp_sleep_wakeup_cause_t wakeupReason = esp_sleep_get_wakeup_cause();
    switch (wakeupReason) {
        case ESP_SLEEP_WAKEUP_EXT0:
            return "External GPIO13 trigger (sensor)";
        case ESP_SLEEP_WAKEUP_TIMER:
            return "Timer wakeup";
        default:
            return "Power-on or reset";
    }
}

// ==================================
// SECTION 4: HARDWARE INITIALIZATION (FAULT TOLERANT)
// ==================================

bool initCamera() {
    Serial.println("[CAMERA] Attempting initialization...");
    
    camera_config_t config;
    config.ledc_channel = LEDC_CHANNEL_0;
    config.ledc_timer = LEDC_TIMER_0;
    
    // Pin assignments for GOOUUU board
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
    
    // Camera parameters
    config.xclk_freq_hz = 20000000;
    config.pixel_format = PIXFORMAT_RGB565;
    config.frame_size = FRAMESIZE_QVGA;
    config.jpeg_quality = 12;
    config.fb_count = 1;
    
    // Use PSRAM if available
    if (psramFound()) {
        config.fb_location = CAMERA_FB_IN_PSRAM;
        config.grab_mode = CAMERA_GRAB_LATEST;
    } else {
        config.fb_location = CAMERA_FB_IN_DRAM;
        config.grab_mode = CAMERA_GRAB_WHEN_EMPTY;
    }
    
    esp_err_t err = esp_camera_init(&config);
    if (err != ESP_OK) {
        device_state.camera_init_error = err;
        Serial.printf("[CAMERA] ❌ FAILED: 0x%x (%s)\n", err, getErrorMessage(err).c_str());
        Serial.println("[CAMERA] Continuing without camera - inference will not work");
        device_state.camera_initialized = false;
        return false;
    }
    
    device_state.camera_initialized = true;
    Serial.println("[CAMERA] ✓ Successfully initialized");
    return true;
}

// Capture image for model inference (returns false gracefully if camera unavailable)
bool captureImageForModel() {
    if (!device_state.camera_initialized) {
        device_state.last_capture_error = 1;
        Serial.println("[CAPTURE] ❌ Camera not initialized");
        return false;
    }
    
    camera_fb_t *fb = esp_camera_fb_get();
    if (!fb) {
        device_state.last_capture_error = 2;
        Serial.println("[CAPTURE] ❌ Frame capture failed");
        return false;
    }
    
    // Allocate buffer for model input
    if (!snapshot_buf) {
        snapshot_buf = (uint8_t *)ei_malloc(EI_CLASSIFIER_INPUT_WIDTH * EI_CLASSIFIER_INPUT_HEIGHT * 3);
        if (!snapshot_buf) {
            device_state.last_capture_error = 3;
            Serial.println("[CAPTURE] ❌ Memory allocation failed");
            esp_camera_fb_return(fb);
            return false;
        }
    }
    
    // Crop and resize to model input size
    int src_width = fb->width;
    int src_height = fb->height;
    int crop_size = min(src_width, src_height);
    int crop_x = (src_width - crop_size) / 2;
    int crop_y = (src_height - crop_size) / 2;
    
    uint16_t *src = (uint16_t *)fb->buf;
    for (int y = 0; y < EI_CLASSIFIER_INPUT_HEIGHT; y++) {
        for (int x = 0; x < EI_CLASSIFIER_INPUT_WIDTH; x++) {
            int src_x = crop_x + (x * crop_size) / EI_CLASSIFIER_INPUT_WIDTH;
            int src_y = crop_y + (y * crop_size) / EI_CLASSIFIER_INPUT_HEIGHT;
            uint16_t pixel = src[src_y * src_width + src_x];
            uint8_t r = ((pixel >> 11) & 0x1F) << 3;
            uint8_t g = ((pixel >> 5) & 0x3F) << 2;
            uint8_t b = (pixel & 0x1F) << 3;
            int dst_index = (y * EI_CLASSIFIER_INPUT_WIDTH + x) * 3;
            snapshot_buf[dst_index + 0] = r;
            snapshot_buf[dst_index + 1] = g;
            snapshot_buf[dst_index + 2] = b;
        }
    }
    
    // Create JPEG for client
    if (jpg_buf) { free(jpg_buf); jpg_buf = nullptr; jpg_len = 0; }
    fmt2jpg(fb->buf, fb->len, fb->width, fb->height, PIXFORMAT_RGB565, 60, &jpg_buf, &jpg_len);
    
    esp_camera_fb_return(fb);
    device_state.last_capture_error = 0;
    Serial.println("[CAPTURE] ✓ Frame captured");
    return true;
}

// ==================================
// SECTION 5: WEB SERVER ENDPOINTS (FAULT TOLERANT)
// ==================================

// Handle inference request - gracefully handles missing camera
void handleRunInference() {
    Serial.println("[API] GET /run - Inference requested");
    
    // Check if camera is available
    if (!device_state.camera_initialized) {
        Serial.println("[API] ❌ Camera not available");
        server.send(503, "application/json", 
            "{\"ok\":false,\"error\":\"Camera not initialized\",\"details\":\"Error 0x" 
            + String(device_state.camera_init_error, HEX) + "\"}");
        return;
    }
    
    // Attempt capture
    if (!captureImageForModel()) {
        Serial.println("[API] ❌ Capture failed");
        String error_msg = "{\"ok\":false,\"error\":\"Capture failed\",\"code\":" 
            + String(device_state.last_capture_error) + "}";
        server.send(500, "application/json", error_msg);
        return;
    }
    
    // Run inference
    unsigned long start_ms = millis();
    signal_t signal;
    signal.total_length = EI_CLASSIFIER_RAW_SAMPLE_COUNT;
    signal.get_data = &get_data;
    ei_impulse_result_t result = {0};
    
    EI_IMPULSE_ERROR res = run_classifier(&signal, &result, false);
    unsigned long inference_ms = millis() - start_ms;
    device_state.last_inference_ms = inference_ms;
    
    if (res != EI_IMPULSE_OK) {
        Serial.printf("[API] ❌ Inference failed: %d\n", res);
        server.send(500, "application/json", 
            "{\"ok\":false,\"error\":\"Inference failed\",\"code\":" + String(res) + "}");
        return;
    }
    
    // Find top classification
    int top_idx = 0;
    for (int i = 1; i < EI_CLASSIFIER_LABEL_COUNT; i++) {
        if (result.classification[i].value > result.classification[top_idx].value) {
            top_idx = i;
        }
    }
    
    // Build detailed response
    String json = "{";
    json += "\"ok\":true,";
    json += "\"top_label\":\"" + String(result.classification[top_idx].label) + "\",";
    json += "\"top_score\":" + String(result.classification[top_idx].value, 4) + ",";
    json += "\"inference_ms\":" + String(result.timing.classification) + ",";
    json += "\"labels\":{";
    for (int i = 0; i < EI_CLASSIFIER_LABEL_COUNT; i++) {
        if (i > 0) json += ",";
        json += "\"" + String(result.classification[i].label) + "\":" 
            + String(result.classification[i].value, 4);
    }
    json += "}}";
    
    server.send(200, "application/json", json);
    Serial.printf("[API] ✓ Inference complete: %s (%.2f) in %lums\n", 
        result.classification[top_idx].label, result.classification[top_idx].value, inference_ms);
}

// Get last captured image
void handleGetImage() {
    Serial.println("[API] GET /image.jpg requested");
    
    if (!jpg_buf || jpg_len == 0) {
        if (!device_state.camera_initialized) {
            server.send(503, "application/json", 
                "{\"error\":\"Camera not initialized\"}");
        } else {
            server.send(404, "application/json", 
                "{\"error\":\"No image available - run inference first\"}");
        }
        return;
    }
    
    server.send_P(200, "image/jpeg", (const char *)jpg_buf, jpg_len);
    Serial.printf("[API] ✓ Sent JPEG (%d bytes)\n", jpg_len);
}

// Get device configuration
void handleGetConfig() {
    Serial.println("[API] GET /get_config");
    
    String json = "{";
    json += "\"ok\":true,";
    json += "\"ssid\":\"" + String(wifi_ssid) + "\",";
    json += "\"sleep_sec\":" + String(sleep_duration_sec) + ",";
    json += "\"person_thresh\":" + String(person_threshold, 2) + ",";
    json += "\"boot_count\":" + String(bootCount) + ",";
    json += "\"wakeup_reason\":\"" + getWakeupReasonText() + "\",";
    json += "\"uptime_sec\":" + String(getUptimeSeconds());
    json += "}";
    
    server.send(200, "application/json", json);
}

// Set device configuration
void handleSetConfig() {
    Serial.println("[API] POST /set_config");
    
    if (server.hasArg("ssid")) {
        server.arg("ssid").toCharArray(wifi_ssid, sizeof(wifi_ssid));
        Serial.printf("[CONFIG] WiFi SSID: %s\n", wifi_ssid);
    }
    if (server.hasArg("password")) {
        server.arg("password").toCharArray(wifi_password, sizeof(wifi_password));
        Serial.println("[CONFIG] WiFi password updated");
    }
    if (server.hasArg("sleep_sec")) {
        sleep_duration_sec = server.arg("sleep_sec").toInt();
        Serial.printf("[CONFIG] Sleep duration: %lu sec\n", sleep_duration_sec);
    }
    if (server.hasArg("person_thresh")) {
        person_threshold = server.arg("person_thresh").toFloat();
        Serial.printf("[CONFIG] Person threshold: %.2f\n", person_threshold);
    }
    
    server.send(200, "application/json", "{\"ok\":true,\"updated\":true}");
}

// Request deep sleep
void handleSleep() {
    Serial.println("[API] POST /sleep");
    
    int duration = sleep_duration_sec;
    if (server.hasArg("duration")) {
        duration = server.arg("duration").toInt();
    }
    
    String response = "{\"ok\":true,\"sleeping_for\":" + String(duration) + "}";
    server.send(200, "application/json", response);
    
    delay(500);
    goToDeepSleep(duration);
}

// NEW: Device health check
void handleHealth() {
    Serial.println("[API] GET /health");
    
    String json = "{";
    json += "\"ok\":true,";
    json += "\"wifi_connected\":" + String(device_state.wifi_connected ? "true" : "false") + ",";
    json += "\"camera_ready\":" + String(device_state.camera_initialized ? "true" : "false") + ",";
    json += "\"uptime_sec\":" + String(getUptimeSeconds()) + ",";
    json += "\"heap_free\":" + String(ESP.getFreeHeap()) + ",";
    json += "\"boot_count\":" + String(bootCount);
    json += "}";
    
    server.send(200, "application/json", json);
}

// NEW: Sensor status endpoint
void handleSensorStatus() {
    Serial.println("[API] GET /sensor_status");
    
    String json = "{";
    json += "\"pir\":{";
    json += "\"available\":" + String(device_state.pir_available ? "true" : "false") + ",";
    json += "\"last_state\":" + String(device_state.pir_last_state) + ",";
    json += "\"description\":\"GROVE PIR motion sensor\"";
    json += "},";
    json += "\"hlk_ld2451\":{";
    json += "\"available\":" + String(device_state.hlk_available ? "true" : "false") + ",";
    json += "\"detection_state\":" + String(device_state.hlk_detection_state) + ",";
    json += "\"last_distance_mm\":" + String(device_state.hlk_last_distance_mm) + ",";
    json += "\"description\":\"HLK-LD2451 millimeter-wave radar\"";
    json += "}";
    json += "}";
    
    server.send(200, "application/json", json);
}

// NEW: Diagnostic information
void handleDiagnose() {
    Serial.println("[API] GET /diagnose");
    
    String json = "{";
    json += "\"device_info\":{";
    json += "\"mac_address\":\"" + WiFi.macAddress() + "\",";
    json += "\"local_ip\":\"" + WiFi.localIP().toString() + "\",";
    json += "\"signal_strength\":" + String(WiFi.RSSI()) + ",";
    json += "\"uptime_sec\":" + String(getUptimeSeconds()) + ",";
    json += "\"boot_count\":" + String(bootCount);
    json += "},";
    json += "\"hardware\":{";
    json += "\"camera_initialized\":" + String(device_state.camera_initialized ? "true" : "false") + ",";
    json += "\"camera_init_error\":\"0x" + String(device_state.camera_init_error, HEX) + "\",";
    json += "\"last_capture_error\":" + String(device_state.last_capture_error) + ",";
    json += "\"last_inference_ms\":" + String(device_state.last_inference_ms);
    json += "},";
    json += "\"memory\":{";
    json += "\"heap_free\":" + String(ESP.getFreeHeap()) + ",";
    json += "\"heap_total\":" + String(ESP.getHeapSize()) + ",";
    json += "\"psram_free\":" + String(ESP.getFreePsram()) + ",";
    json += "\"psram_total\":" + String(ESP.getPsramSize());
    json += "}";
    json += "}";
    
    server.send(200, "application/json", json);
}

// ==================================
// SECTION 6: INITIALIZATION & RUNTIME
// ==================================

void setup() {
    // Initialize Serial FIRST and wait for full stabilization
    Serial.begin(460800);
    delay(3000);  // Full 3-second delay for UART stability (same as reference firmware)
    Serial.flush();
    
    // NOW configure brown-out register (after serial is stable)
    WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0);
    
    // Now safe to print
    Serial.println("\n\n[BOOT:0] Starting setup()");
    Serial.println("[BOOT:1] Brown-out register disabled");
    Serial.println("[BOOT:2] Serial initialized at 460800");
    
    boot_time_ms = millis();
    Serial.println("[BOOT:3] boot_time_ms set");
    bootCount++;
    Serial.println("[BOOT:4] bootCount incremented");
    
    Serial.println("\n╔════════════════════════════════════════╗");
    Serial.println("║   ESP32-S3 Person Detection System    ║");
    Serial.println("║   Robust & Fault Tolerant             ║");
    Serial.println("╚════════════════════════════════════════╝\n");
    
    Serial.printf("[BOOT:5] Boot count: %d\n", bootCount);
    Serial.printf("[BOOT:6] Wakeup reason: %s\n", getWakeupReasonText().c_str());
    Serial.printf("[BOOT:7] Heap free: %u bytes\n", ESP.getFreeHeap());
    Serial.println("[BOOT:8] About to init UART2");
    
    // ============== SECTION 6.0B: HLK-LD2451 UART INITIALIZATION ==============
    Serial.println("\n[INIT:0] ▶ HLK-LD2451 UART2 Setup (115200 baud)");
    // TEMPORARILY DISABLED - debugging crash
    // Serial2.begin(HLK_LD2451_BAUD, SERIAL_8N1, HLK_LD2451_RX_PIN, HLK_LD2451_TX_PIN);
    // Serial.println("[INIT] ✓ UART2 initialized for HLK-LD2451");
    Serial.println("[INIT:1] ⚠️  UART2 disabled for debugging");
    
    // ============== SECTION 6.1: WIFI INITIALIZATION ==============
    Serial.println("\n[WIFI:0] ▶ WiFi Initialization");
    if (strlen(wifi_ssid) == 0) {
        Serial.println("[WIFI:1] SSID empty, using fallback");
        strcpy(wifi_ssid, WIFI_SSID_FALLBACK);
        strcpy(wifi_password, WIFI_PASSWORD_FALLBACK);
        Serial.println("[WIFI:2] Fallback credentials set");
    }
    
    Serial.println("[WIFI:3] About to call WiFi.begin()");
    WiFi.begin(wifi_ssid, wifi_password);
    Serial.println("[WIFI:4] WiFi.begin() returned successfully");
    Serial.printf("[WIFI:5] Connecting to '%s' ", wifi_ssid);
    
    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 20) {
        delay(500);
        Serial.print(".");
        attempts++;
    }
    Serial.println();
    
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("[INIT] ❌ WiFi connection FAILED");
        Serial.println("[INIT] Entering deep sleep - will retry on next wakeup");
        delay(500);
        goToDeepSleep(sleep_duration_sec);
        return;
    }
    
    device_state.wifi_connected = true;
    Serial.println("[WIFI:6] device_state.wifi_connected set to true");
    Serial.printf("[WIFI:7] ✓ WiFi connected! IP: %s\n", WiFi.localIP().toString().c_str());
    Serial.printf("[WIFI:8] Signal strength: %d dBm\n", WiFi.RSSI());
    
    // ============== SECTION 6.2: HARDWARE INITIALIZATION (NON-FATAL) ==============
    Serial.println("\n[CAMERA:0] ▶ Hardware Initialization (continue if failures)");
    Serial.println("[CAMERA:1] About to call initCamera()");
    
    // Try to initialize camera - DON'T crash if it fails
    if (!initCamera()) {
        Serial.println("[CAMERA:2] initCamera() returned false");
        Serial.println("[CAMERA:3] ⚠️  Camera initialization failed");
        Serial.println("[CAMERA:4] Device will continue without inference capability");
        device_state.camera_initialized = false;
    } else {
        Serial.println("[CAMERA:2] initCamera() returned true");
        Serial.println("[CAMERA:5] ✓ Camera ready for inference");
    }
    
    // ============== SECTION 6.2B: SENSOR INITIALIZATION ==============
    Serial.println("\n[SENSORS:0] ▶ Sensor Initialization");
    // TEMPORARILY DISABLED - debugging crash
    // initSensors();
    Serial.println("[SENSORS:1] ⚠️  Sensors disabled for debugging");
    
    // ============== SECTION 6.3: WEB SERVER SETUP ==============
    Serial.println("\n[SERVER:0] ▶ Web Server Setup");
    
    // Core endpoints
    Serial.println("[SERVER:1] Registering /run endpoint");
    server.on("/run", HTTP_GET, handleRunInference);
    Serial.println("[SERVER:2] Registering /image.jpg endpoint");
    server.on("/image.jpg", HTTP_GET, handleGetImage);
    Serial.println("[SERVER:3] Registering /get_config endpoint");
    server.on("/get_config", HTTP_GET, handleGetConfig);
    Serial.println("[SERVER:4] Registering /set_config endpoint");
    server.on("/set_config", HTTP_POST, handleSetConfig);
    Serial.println("[SERVER:5] Registering /sleep endpoint");
    server.on("/sleep", HTTP_POST, handleSleep);
    
    // Diagnostic endpoints (always available)
    Serial.println("[SERVER:6] Registering /health endpoint");
    server.on("/health", HTTP_GET, handleHealth);
    Serial.println("[SERVER:7] Registering /sensor_status endpoint");
    server.on("/sensor_status", HTTP_GET, handleSensorStatus);
    Serial.println("[SERVER:8] Registering /diagnose endpoint");
    server.on("/diagnose", HTTP_GET, handleDiagnose);
    
    Serial.println("[SERVER:9] About to call server.begin()");
    server.begin();
    Serial.println("[SERVER:10] ✓ Web server started on port 80");
    
    // ============== SECTION 6.4: STARTUP SUMMARY ==============
    Serial.println("\n[STARTUP:0] Printing summary...");
    Serial.println("\n╔════════════════════════════════════════╗");
    Serial.println("║   System Ready - Available Endpoints   ║");
    Serial.println("╠════════════════════════════════════════╣");
    Serial.println("║ Core Endpoints:                        ║");
    Serial.println("║  GET  /run              - Run inference║");
    Serial.println("║  GET  /image.jpg        - Get last img ║");
    Serial.println("║  GET  /get_config       - Get settings ║");
    Serial.println("║  POST /set_config       - Set settings ║");
    Serial.println("║  POST /sleep            - Request sleep║");
    Serial.println("║                                        ║");
    Serial.println("║ Diagnostic Endpoints (always work):    ║");
    Serial.println("║  GET  /health           - System status║");
    Serial.println("║  GET  /diagnose         - Full details ║");
    Serial.println("╚════════════════════════════════════════╝\n");
    Serial.println("[STARTUP:1] Summary printed");
    
    if (!device_state.camera_initialized) {
        Serial.println("[!] ⚠️  NOTE: Camera not detected - /run will return error");
        Serial.println("[!] Ensure camera is properly connected and try /health endpoint\n");
    }
    Serial.println("[STARTUP:2] About to check wakeup cause");
    
    // If woken by external sensor, run inference
    if (esp_sleep_get_wakeup_cause() == ESP_SLEEP_WAKEUP_EXT0 && device_state.camera_initialized) {
        Serial.println("[BOOT] Woken by external sensor - running inference automatically...\n");
        handleRunInference();
    }
    
    Serial.println("\n[SETUP:SUCCESS] ✓✓✓ setup() completed successfully! ✓✓✓\n");
}

void loop() {
    // ============== SENSOR TRIGGER LOOP ==============
    Serial.println("[LOOP:0] Starting main loop iteration");
    // Check if sensors indicate motion/object - if so, run inference
    static unsigned long last_inference_trigger_time = 0;
    const unsigned long INFERENCE_COOLDOWN_MS = 2000; // Avoid too-frequent triggers
    
    // Check sensors only if camera is ready and enough time passed
    if (device_state.camera_initialized && 
        (millis() - last_inference_trigger_time) > INFERENCE_COOLDOWN_MS) {
        
        // Check for sensor trigger
        if (shouldRunInference()) {
            Serial.println("\n[FLOW] 🔴 SENSOR TRIGGERED - Starting inference pipeline");
            
            // Capture image from camera
            uint8_t capture_result = captureImageForModel();
            if (capture_result == 0) {
                Serial.println("[FLOW] ✓ Image captured successfully");
                
                // Run inference on captured image
                handleRunInference();
                
                last_inference_trigger_time = millis();
            } else {
                Serial.printf("[FLOW] ❌ Image capture failed: code %d\n", capture_result);
            }
        }
    }
    
    // ============== WEB SERVER HANDLING ==============
    server.handleClient();
    delay(10);
}