#ifndef CONFIG_H
#define CONFIG_H

// ==================================
// Board and Camera Pin Definitions
// ==================================

// Using GOOUUU ESP32-S3-CAM board
#define CAM_BOARD_GOOUUU
#if defined(CAM_BOARD_GOOUUU)
  #define PWDN_GPIO_NUM     -1
  #define RESET_GPIO_NUM    -1
  #define XCLK_GPIO_NUM     15
  #define SIOD_GPIO_NUM      4
  #define SIOC_GPIO_NUM      5
  #define Y9_GPIO_NUM       16
  #define Y8_GPIO_NUM       17
  #define Y7_GPIO_NUM       18
  #define Y6_GPIO_NUM       12
  #define Y5_GPIO_NUM       10
  #define Y4_GPIO_NUM        8
  #define Y3_GPIO_NUM        9
  #define Y2_GPIO_NUM       11
  #define VSYNC_GPIO_NUM     6
  #define HREF_GPIO_NUM      7
  #define PCLK_GPIO_NUM     13
#else
  #error "No camera board pins defined."
#endif

// ==================================
// Project specific Pins

// GPIO21 receives the digital output from the Grove PIR sensor.
#define PIR_WAKEUP_PIN GPIO_NUM_21

// GPIO3 receives the digital OT1 output from the HLK sensor.
#define HLK_WAKEUP_PIN GPIO_NUM_3

// Sensor Pins

// The PIR signal pin is also used as a deep sleep wakeup source.
#define GROVE_PIR_PIN PIR_WAKEUP_PIN

// HLK LD2451 UART configuration
// UART communication is currently disabled.
// The controller wakes from the HLK digital OT1 output on GPIO3.
#define HLK_LD2451_UART_NUM UART_NUM_2
#define HLK_LD2451_RX_PIN GPIO_NUM_14
#define HLK_LD2451_BAUD 115200
// Sensor Enable Pin (optional)
#define SENSOR_ENABLE_PIN GPIO_NUM_2

// ==================================
// WiFi Credentials
// ==================================
// These are fallback credentials. The Python client can override them.
const char* WIFI_SSID_FALLBACK = "Amit_wifi";
const char* WIFI_PASSWORD_FALLBACK = "Amit1998";

#endif // CONFIG_H
