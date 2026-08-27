#include <WiFi.h>
#include <PubSubClient.h>
#include "esp_camera.h"

// ---- Wi-Fi ----
const char* WIFI_SSID     = "YOUR_WIFI_SSID";
const char* WIFI_PASSWORD = "YOUR_WIFI_PASSWORD";

// ---- MQTT ----
const char* MQTT_BROKER   = "your-vps-ip-or-hostname";
const int   MQTT_PORT     = 1883;
const char* MQTT_USER     = "";      // leave blank if broker has no auth
const char* MQTT_PASS     = "";
const char* CLIENT_ID     = "esp32cam-1";

const char* TOPIC_CAM_IMAGE  = "greenflow/cam1/image";
const char* TOPIC_CAM_STATUS = "greenflow/cam1/status";

const unsigned long CAPTURE_INTERVAL_MS = 3000; // send a frame every 3s

// ---- AI-Thinker ESP32-CAM pin map ----
#define PWDN_GPIO_NUM   32
#define RESET_GPIO_NUM  -1
#define XCLK_GPIO_NUM    0
#define SIOD_GPIO_NUM   26
#define SIOC_GPIO_NUM   27
#define Y9_GPIO_NUM     35
#define Y8_GPIO_NUM     34
#define Y7_GPIO_NUM     39
#define Y6_GPIO_NUM     36
#define Y5_GPIO_NUM     21
#define Y4_GPIO_NUM     19
#define Y3_GPIO_NUM     18
#define Y2_GPIO_NUM      5
#define VSYNC_GPIO_NUM  25
#define HREF_GPIO_NUM   23
#define PCLK_GPIO_NUM   22

WiFiClient espClient;
PubSubClient mqttClient(espClient);
unsigned long lastCapture = 0;

void setupWifi() {
  Serial.printf("Connecting to Wi-Fi: %s\n", WIFI_SSID);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.printf("\nWi-Fi connected, IP=%s\n", WiFi.localIP().toString().c_str());
}

void setupCamera() {
  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer   = LEDC_TIMER_0;
  config.pin_d0 = Y2_GPIO_NUM;  config.pin_d1 = Y3_GPIO_NUM;
  config.pin_d2 = Y4_GPIO_NUM;  config.pin_d3 = Y5_GPIO_NUM;
  config.pin_d4 = Y6_GPIO_NUM;  config.pin_d5 = Y7_GPIO_NUM;
  config.pin_d6 = Y8_GPIO_NUM;  config.pin_d7 = Y9_GPIO_NUM;
  config.pin_xclk = XCLK_GPIO_NUM;
  config.pin_pclk = PCLK_GPIO_NUM;
  config.pin_vsync = VSYNC_GPIO_NUM;
  config.pin_href = HREF_GPIO_NUM;
  config.pin_sccb_sda = SIOD_GPIO_NUM;
  config.pin_sccb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;
  config.xclk_freq_hz = 20000000;
  config.pixel_format = PIXFORMAT_JPEG;

  // Keep frames small so they fit comfortably in one MQTT message.
  config.frame_size = FRAMESIZE_QVGA;   // 320x240
  config.jpeg_quality = 12;             // lower number = higher quality/larger file
  config.fb_count = 1;

  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Camera init failed: 0x%x\n", err);
    while (true) delay(1000);
  }
}

void reconnectMqtt() {
  while (!mqttClient.connected()) {
    Serial.print("Connecting to MQTT broker...");
    bool ok;
    if (strlen(MQTT_USER) > 0) {
      ok = mqttClient.connect(CLIENT_ID, MQTT_USER, MQTT_PASS,
                               TOPIC_CAM_STATUS, 0, true, "offline");
    } else {
      ok = mqttClient.connect(CLIENT_ID, TOPIC_CAM_STATUS, 0, true, "offline");
    }
    if (ok) {
      Serial.println("connected");
      mqttClient.publish(TOPIC_CAM_STATUS, "online", true);
    } else {
      Serial.printf("failed, rc=%d, retrying in 2s\n", mqttClient.state());
      delay(2000);
    }
  }
}

void setup() {
  Serial.begin(115200);
  setupWifi();
  setupCamera();

  mqttClient.setServer(MQTT_BROKER, MQTT_PORT);
  // QVGA/JPEG frames are usually 10-25KB; give PubSubClient enough buffer.
  mqttClient.setBufferSize(32768);
}

void loop() {
  if (!mqttClient.connected()) {
    reconnectMqtt();
  }
  mqttClient.loop();

  unsigned long now = millis();
  if (now - lastCapture >= CAPTURE_INTERVAL_MS) {
    lastCapture = now;

    camera_fb_t* fb = esp_camera_fb_get();
    if (!fb) {
      Serial.println("Camera capture failed");
      return;
    }

    bool published = mqttClient.publish(TOPIC_CAM_IMAGE, fb->buf, fb->len);
    Serial.printf("Published frame (%u bytes): %s\n", fb->len, published ? "ok" : "FAILED (increase buffer/broker max_packet_size)");

    esp_camera_fb_return(fb);
  }
}
