// ====== 1. INCLUDES ======
#include <object_detection_inferencing.h>
#include "esp_camera.h"

// ====== 2. AI THINKER CAMERA PINS ======
#define PWDN_GPIO_NUM     32
#define RESET_GPIO_NUM    -1
#define XCLK_GPIO_NUM      0
#define SIOD_GPIO_NUM     26
#define SIOC_GPIO_NUM     27
#define Y9_GPIO_NUM       35
#define Y8_GPIO_NUM       34
#define Y7_GPIO_NUM       39
#define Y6_GPIO_NUM       36
#define Y5_GPIO_NUM       21
#define Y4_GPIO_NUM       19
#define Y3_GPIO_NUM       18
#define Y2_GPIO_NUM        5
#define VSYNC_GPIO_NUM    25
#define HREF_GPIO_NUM     23
#define PCLK_GPIO_NUM     22

// ====== 3. GLOBALS ======
#define IMG_WIDTH   96
#define IMG_HEIGHT  96

static uint8_t snapshot_buf[IMG_WIDTH * IMG_HEIGHT];

// ====== 4. CAMERA INIT ======
void initCamera() {
    camera_config_t config;
    config.ledc_channel = LEDC_CHANNEL_0;
    config.ledc_timer   = LEDC_TIMER_0;
    config.pin_d0       = Y2_GPIO_NUM;
    config.pin_d1       = Y3_GPIO_NUM;
    config.pin_d2       = Y4_GPIO_NUM;
    config.pin_d3       = Y5_GPIO_NUM;
    config.pin_d4       = Y6_GPIO_NUM;
    config.pin_d5       = Y7_GPIO_NUM;
    config.pin_d6       = Y8_GPIO_NUM;
    config.pin_d7       = Y9_GPIO_NUM;
    config.pin_xclk     = XCLK_GPIO_NUM;
    config.pin_pclk     = PCLK_GPIO_NUM;
    config.pin_vsync    = VSYNC_GPIO_NUM;
    config.pin_href     = HREF_GPIO_NUM;
    config.pin_sccb_sda = SIOD_GPIO_NUM;
    config.pin_sccb_scl = SIOC_GPIO_NUM;
    config.pin_pwdn     = PWDN_GPIO_NUM;
    config.pin_reset    = RESET_GPIO_NUM;
    config.xclk_freq_hz = 20000000;
    config.pixel_format = PIXFORMAT_GRAYSCALE;
    config.frame_size   = FRAMESIZE_96X96;
    config.fb_count     = 1;

    esp_err_t err = esp_camera_init(&config);
    if (err != ESP_OK) {
        Serial.printf("Camera init failed: 0x%x\n", err);
        while (true) delay(1000);
    }
    Serial.println("Camera ready!");
}

// ====== 5. EDGE IMPULSE CALLBACK ======
int get_data(size_t offset, size_t length, float *out_ptr) {
    for (size_t i = 0; i < length; i++) {
        // Grayscale: same value for R, G, B packed into float
        uint8_t pixel = snapshot_buf[offset + i];
        out_ptr[i] = (pixel << 16) | (pixel << 8) | pixel;
    }
    return 0;
}

// ====== 6. SETUP ======
void setup() {
    Serial.begin(115200);
    Serial.println("\n=== Human Detection System ===");
    initCamera();
}

// ====== 7. LOOP ======
void loop() {
    // Capture
    camera_fb_t *fb = esp_camera_fb_get();
    if (!fb) {
        Serial.println("Capture failed!");
        delay(1000);
        return;
    }

    // Copy to buffer
    memcpy(snapshot_buf, fb->buf, IMG_WIDTH * IMG_HEIGHT);
    esp_camera_fb_return(fb);

    // Run inference
    ei_impulse_result_t result = {0};
    signal_t signal;
    signal.total_length = IMG_WIDTH * IMG_HEIGHT;
    signal.get_data = &get_data;

    EI_IMPULSE_ERROR res = run_classifier(&signal, &result, false);
    if (res != EI_IMPULSE_OK) {
        Serial.printf("Classifier error: %d\n", res);
        delay(1000);
        return;
    }

    // Print results
    Serial.printf("Inference: %d ms\n", result.timing.classification);
    for (int i = 0; i < EI_CLASSIFIER_LABEL_COUNT; i++) {
        Serial.printf("  %s: %.2f\n",
            result.classification[i].label,
            result.classification[i].value);
    }
    Serial.println();

    delay(3000);
}