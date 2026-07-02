/*define in tools:
Board: ESP32S3 Dev Module
Port: הפורט של הבקר, למשל COM9

USB CDC On Boot: Disabled
CPU Frequency: 240MHz
Flash Size: 4MB
Partition Scheme: Huge APP
PSRAM: OPI PSRAM
Flash Mode: DIO
Upload Speed: 115200
Erase All Flash Before Sketch Upload: Enabled*/

#include "config.h"

#include <WiFi.h>
#include <WebServer.h>

#include "esp_sleep.h"
#include "driver/rtc_io.h"
#include "esp_camera.h"
#include "img_converters.h"

RTC_DATA_ATTR int bootCount = 0;
RTC_DATA_ATTR bool nextFakeResultIsHuman = true;

WebServer server(80);

static uint8_t *jpg_buf = nullptr;
static size_t jpg_len = 0;

bool cameraInitialized = false;
bool imageWasDownloaded = false;

bool humanDetected = false;
float humanProbability = 0.0;
String aiMessage = "";

unsigned long serverStartTime = 0;
unsigned long imageDownloadTime = 0;

const unsigned long SERVER_TIMEOUT_MS = 30000;
const unsigned long SLEEP_AFTER_DOWNLOAD_MS = 2000;


// ==================================================
// Wakeup reason
// ==================================================

String getWakeupReasonText() {
    esp_sleep_wakeup_cause_t reason =
        esp_sleep_get_wakeup_cause();

    if (reason == ESP_SLEEP_WAKEUP_EXT1) {
        uint64_t status =
            esp_sleep_get_ext1_wakeup_status();

        if (status & (1ULL << PIR_WAKEUP_PIN)) {
            return "GPIO21 button trigger";
        }

        return "External GPIO trigger";
    }

    return "Power on or reset";
}


// ==================================================
// Simulated AI model
// ==================================================

void runFakeAiModel() {
    Serial.println();
    Serial.println("[AI] Running simulated AI model");

    if (nextFakeResultIsHuman) {
        humanDetected = true;
        humanProbability = 90.0;

        aiMessage =
            "Using AI model: detected human with probability 90%";
    } else {
        humanDetected = false;
        humanProbability = 60.0;

        aiMessage =
            "Using AI model: no human detected with probability 60%";
    }

    nextFakeResultIsHuman = !nextFakeResultIsHuman;

    Serial.print("[AI] ");
    Serial.println(aiMessage);
}


// ==================================================
// Camera initialization
// ==================================================

bool initCamera() {
    Serial.println("[CAMERA] Initializing camera...");

    camera_config_t config = {};

    config.ledc_channel = LEDC_CHANNEL_0;
    config.ledc_timer = LEDC_TIMER_0;

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

    config.pin_sccb_sda = SIOD_GPIO_NUM;
    config.pin_sccb_scl = SIOC_GPIO_NUM;

    config.pin_pwdn = PWDN_GPIO_NUM;
    config.pin_reset = RESET_GPIO_NUM;

    config.xclk_freq_hz = 20000000;
    config.pixel_format = PIXFORMAT_RGB565;
    config.frame_size = FRAMESIZE_QVGA;
    config.jpeg_quality = 12;
    config.fb_count = 1;

    if (psramFound()) {
        Serial.println("[CAMERA] PSRAM found");

        config.fb_location = CAMERA_FB_IN_PSRAM;
        config.grab_mode = CAMERA_GRAB_LATEST;
    } else {
        Serial.println("[CAMERA] PSRAM not found");

        config.fb_location = CAMERA_FB_IN_DRAM;
        config.grab_mode = CAMERA_GRAB_WHEN_EMPTY;
    }

    esp_err_t result = esp_camera_init(&config);

    if (result != ESP_OK) {
        Serial.printf(
            "[CAMERA] Initialization failed: 0x%x\n",
            result
        );

        return false;
    }

    cameraInitialized = true;

    Serial.println(
        "[CAMERA] Camera initialized successfully"
    );

    return true;
}


// ==================================================
// Capture one image
// ==================================================

bool captureOneImage() {
    Serial.println("[CAPTURE] Capturing one frame...");

    camera_fb_t *frame = esp_camera_fb_get();

    if (!frame) {
        Serial.println(
            "[CAPTURE] Frame capture failed"
        );

        return false;
    }

    Serial.printf(
        "[CAPTURE] Raw frame: %ux%u, %u bytes\n",
        frame->width,
        frame->height,
        frame->len
    );

    if (jpg_buf != nullptr) {
        free(jpg_buf);

        jpg_buf = nullptr;
        jpg_len = 0;
    }

    bool converted = fmt2jpg(
        frame->buf,
        frame->len,
        frame->width,
        frame->height,
        PIXFORMAT_RGB565,
        60,
        &jpg_buf,
        &jpg_len
    );

    esp_camera_fb_return(frame);

    if (!converted || jpg_buf == nullptr || jpg_len == 0) {
        Serial.println(
            "[CAPTURE] JPEG conversion failed"
        );

        return false;
    }

    Serial.println(
        "[CAPTURE] Image captured successfully"
    );

    Serial.printf(
        "[CAPTURE] JPEG stored in PSRAM: %u bytes\n",
        jpg_len
    );

    return true;
}


// ==================================================
// WiFi
// ==================================================

bool connectToWiFi() {
    Serial.println();

    Serial.printf(
        "[WIFI] Connecting to %s",
        WIFI_SSID_FALLBACK
    );

    WiFi.mode(WIFI_STA);

    IPAddress localIP(172, 20, 10, 6);
    IPAddress gateway(172, 20, 10, 1);
    IPAddress subnet(255, 255, 255, 240);
    IPAddress dns(172, 20, 10, 1);

    if (!WiFi.config(localIP, gateway, subnet, dns)) {
        Serial.println();

        Serial.println(
            "[WIFI] Static IP configuration failed"
        );

        return false;
    }

    WiFi.begin(
        WIFI_SSID_FALLBACK,
        WIFI_PASSWORD_FALLBACK
    );

    unsigned long connectionStart = millis();

    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");

        if (millis() - connectionStart > 20000) {
            Serial.println();

            Serial.println(
                "[WIFI] Connection timed out"
            );

            return false;
        }
    }

    Serial.println();

    Serial.println(
        "[WIFI] Connected successfully"
    );

    Serial.print(
        "[WIFI] ESP32 static IP address: "
    );

    Serial.println(WiFi.localIP());

    return true;
}


// ==================================================
// Web Server handlers
// ==================================================

void handleRoot() {
    String html;

    html += "<!DOCTYPE html>";
    html += "<html>";
    html += "<head>";
    html += "<meta charset='UTF-8'>";
    html += "<meta name='viewport' content='width=device-width, initial-scale=1.0'>";
    html += "<title>ESP32 Detection Result</title>";

    html += "<style>";
    html += "body{font-family:Arial;text-align:center;margin:30px;}";
    html += "img{max-width:100%;height:auto;border:1px solid #ccc;}";
    html += "#status{margin-top:15px;}";
    html += "</style>";

    html += "</head>";
    html += "<body>";

    html += "<h1>ESP32 Camera Detection System</h1>";

    html += "<h2 id='aiMessage'>";
    html += aiMessage;
    html += "</h2>";

    html += "<p><strong>Human detected:</strong> ";
    html += "<span id='humanDetected'>";
    html += humanDetected ? "YES" : "NO";
    html += "</span></p>";

    html += "<p><strong>Probability:</strong> ";
    html += "<span id='probability'>";
    html += String(humanProbability, 1);
    html += "</span>%</p>";

    html += "<p><strong>Image size:</strong> ";
    html += "<span id='imageSize'>";
    html += String(jpg_len);
    html += "</span> bytes</p>";

    html += "<h3>Captured image</h3>";

    html += "<img id='capturedImage' ";
    html += "src='/image.jpg?t=";
    html += String(millis());
    html += "' alt='Captured image'>";

    html += "<p id='status'>Waiting for updates...</p>";

    html += "<script>";

    html += "async function updatePage(){";
    html += "try{";
    html += "const response=await fetch('/info?t='+Date.now(),";
    html += "{cache:'no-store'});";
    html += "if(!response.ok)throw new Error('offline');";

    html += "const data=await response.json();";

    html += "document.getElementById('aiMessage').textContent=";
    html += "data.ai_message;";

    html += "document.getElementById('humanDetected').textContent=";
    html += "data.human_detected?'YES':'NO';";

    html += "document.getElementById('probability').textContent=";
    html += "data.probability.toFixed(1);";

    html += "document.getElementById('imageSize').textContent=";
    html += "data.jpg_bytes;";

    html += "document.getElementById('capturedImage').src=";
    html += "'/image.jpg?t='+Date.now();";

    html += "document.getElementById('status').textContent=";
    html += "'Updated successfully';";
    html += "}catch(error){";
    html += "document.getElementById('status').textContent=";
    html += "'ESP32 is sleeping or unavailable';";
    html += "}";
    html += "}";

    html += "setInterval(updatePage,3000);";

    html += "</script>";
    html += "</body>";
    html += "</html>";

    server.send(
        200,
        "text/html",
        html
    );
}


void handleInfo() {
    String json = "{";

    json += "\"ok\":true,";

    json += "\"boot_count\":";
    json += String(bootCount);
    json += ",";

    json += "\"image_available\":";
    json +=
        (jpg_buf != nullptr && jpg_len > 0)
        ? "true"
        : "false";
    json += ",";

    json += "\"image_downloaded\":";
    json += imageWasDownloaded
        ? "true"
        : "false";
    json += ",";

    json += "\"jpg_bytes\":";
    json += String(jpg_len);
    json += ",";

    json += "\"human_detected\":";
    json += humanDetected
        ? "true"
        : "false";
    json += ",";

    json += "\"probability\":";
    json += String(humanProbability, 1);
    json += ",";

    json += "\"ai_message\":\"";
    json += aiMessage;
    json += "\",";

    json += "\"ip\":\"";
    json += WiFi.localIP().toString();
    json += "\"";

    json += "}";

    server.send(
        200,
        "application/json",
        json
    );
}


void handleGetImage() {
    Serial.println(
        "[SERVER] GET /image.jpg"
    );

    if (jpg_buf == nullptr || jpg_len == 0) {
        server.send(
            404,
            "application/json",
            "{\"ok\":false,\"error\":\"No image available\"}"
        );

        Serial.println(
            "[SERVER] No image available"
        );

        return;
    }

    server.setContentLength(jpg_len);

    server.send(
        200,
        "image/jpeg",
        ""
    );

    WiFiClient client = server.client();

    size_t bytesSent =
        client.write(jpg_buf, jpg_len);

    Serial.printf(
        "[SERVER] JPEG sent: %u of %u bytes\n",
        bytesSent,
        jpg_len
    );

    if (bytesSent == jpg_len) {
        imageWasDownloaded = true;
        imageDownloadTime = millis();

        Serial.println(
            "[SERVER] Image download completed"
        );

        Serial.println(
            "[SERVER] Controller will sleep in 2 seconds"
        );
    }
}


void startWebServer() {
    server.on(
        "/",
        HTTP_GET,
        handleRoot
    );

    server.on(
        "/info",
        HTTP_GET,
        handleInfo
    );

    server.on(
        "/image.jpg",
        HTTP_GET,
        handleGetImage
    );

    server.onNotFound([]() {
        server.send(
            404,
            "application/json",
            "{\"ok\":false,\"error\":\"Endpoint not found\"}"
        );
    });

    server.begin();

    serverStartTime = millis();

    Serial.println(
        "[SERVER] Web Server started"
    );

    Serial.print(
        "[SERVER] Home: http://"
    );

    Serial.println(WiFi.localIP());

    Serial.print(
        "[SERVER] Image: http://"
    );

    Serial.print(WiFi.localIP());

    Serial.println("/image.jpg");

    Serial.print(
        "[SERVER] Info: http://"
    );

    Serial.print(WiFi.localIP());

    Serial.println("/info");

    Serial.println(
        "[SERVER] Waiting up to 30 seconds for image download"
    );
}


// ==================================================
// Cleanup
// ==================================================

void cleanupBeforeSleep() {
    Serial.println(
        "[SLEEP] Cleaning resources"
    );

    server.stop();

    WiFi.disconnect(true);
    WiFi.mode(WIFI_OFF);

    if (jpg_buf != nullptr) {
        free(jpg_buf);

        jpg_buf = nullptr;
        jpg_len = 0;
    }

    if (cameraInitialized) {
        esp_camera_deinit();
        cameraInitialized = false;
    }

    delay(200);
}


// ==================================================
// Deep sleep
// ==================================================

void goToDeepSleep() {
    Serial.println();

    Serial.println(
        "[SLEEP] Preparing deep sleep"
    );

    cleanupBeforeSleep();

    pinMode(
        PIR_WAKEUP_PIN,
        INPUT_PULLDOWN
    );

    while (digitalRead(PIR_WAKEUP_PIN) == HIGH) {
        Serial.println(
            "[SLEEP] Waiting for GPIO21 to return LOW..."
        );

        delay(100);
    }

    rtc_gpio_pullup_dis(PIR_WAKEUP_PIN);
    rtc_gpio_pulldown_en(PIR_WAKEUP_PIN);

    uint64_t wakeupMask =
        1ULL << PIR_WAKEUP_PIN;

    esp_err_t result =
        esp_sleep_enable_ext1_wakeup(
            wakeupMask,
            ESP_EXT1_WAKEUP_ANY_HIGH
        );

    if (result != ESP_OK) {
        Serial.printf(
            "[SLEEP] Wakeup configuration failed: %d\n",
            result
        );

        return;
    }

    Serial.println(
        "[SLEEP] Wakeup source: GPIO21 HIGH"
    );

    Serial.println(
        "[SLEEP] Entering deep sleep now..."
    );

    Serial.flush();

    delay(500);

    esp_deep_sleep_start();
}


// ==================================================
// Setup
// ==================================================

void setup() {
    Serial.begin(115200);
    delay(1500);

    bootCount++;

    Serial.println();

    Serial.println(
        "================================"
    );

    Serial.println(
        "Sleep, Wake, Camera, Fake AI and Server Test"
    );

    Serial.println(
        "================================"
    );

    esp_sleep_wakeup_cause_t wakeupReason =
        esp_sleep_get_wakeup_cause();

    Serial.printf(
        "[BOOT] Boot count: %d\n",
        bootCount
    );

    Serial.printf(
        "[BOOT] Wakeup reason: %s\n",
        getWakeupReasonText().c_str()
    );

    if (wakeupReason != ESP_SLEEP_WAKEUP_EXT1) {
        Serial.println(
            "[BOOT] Normal startup, entering sleep"
        );

        delay(1000);

        goToDeepSleep();

        return;
    }

    Serial.println(
        "[BOOT] GPIO trigger received"
    );

    if (!initCamera()) {
        Serial.println(
            "[TEST] FAILED: Camera initialization failed"
        );

        delay(3000);

        goToDeepSleep();

        return;
    }

    if (!captureOneImage()) {
        Serial.println(
            "[TEST] FAILED: Image capture failed"
        );

        delay(3000);

        goToDeepSleep();

        return;
    }

    Serial.println();

    Serial.println(
        "[TEST] PASSED: One image captured after trigger"
    );

    runFakeAiModel();

    if (!connectToWiFi()) {
        Serial.println(
            "[TEST] FAILED: WiFi connection failed"
        );

        delay(3000);

        goToDeepSleep();

        return;
    }

    startWebServer();

    Serial.println();

    Serial.println(
        "[TEST] PASSED: Image and AI result are available through Web Server"
    );
}


// ==================================================
// Loop
// ==================================================

void loop() {
    server.handleClient();

    if (
        imageWasDownloaded &&
        millis() - imageDownloadTime >=
        SLEEP_AFTER_DOWNLOAD_MS
    ) {
        Serial.println();

        Serial.println(
            "[FLOW] Image was downloaded"
        );

        Serial.println(
            "[FLOW] Returning to deep sleep"
        );

        goToDeepSleep();
    }

    if (
        !imageWasDownloaded &&
        millis() - serverStartTime >=
        SERVER_TIMEOUT_MS
    ) {
        Serial.println();

        Serial.println(
            "[FLOW] Server timeout reached"
        );

        Serial.println(
            "[FLOW] No image download was detected"
        );

        Serial.println(
            "[FLOW] Returning to deep sleep"
        );

        goToDeepSleep();
    }

    delay(2);
}