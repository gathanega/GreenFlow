#include <Wire.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <UniversalTelegramBot.h>
#include <ArduinoJson.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>

// -- Wifi --
const char* WIFI_SSID     = "YOUR_WIFI_SSID";
const char* WIFI_PASSWORD = "YOUR_WIFI_PASSWORD";

// ---- MQTT ----
const char* MQTT_BROKER   = "your-vps-ip-or-hostname";
const int   MQTT_PORT     = 1883;
const char* MQTT_USER     = "";
const char* MQTT_PASS     = "";
const char* CLIENT_ID     = "esp32c6-1";

const char* TOPIC_LIGHT_CONTROL   = "greenflow/control/lights";
const char* TOPIC_SOLAR_TELEMETRY = "greenflow/solar/telemetry";
const char* TOPIC_C6_STATUS       = "greenflow/c6/status";

// -- Sensor --
const int PIN_LED_RED    = ;
const int PIN_LED_YELLOW = ;
const int PIN_LED_GREEN  = ;
const int PIN_BUZZER     = ;

WiFiClient espClient;
PubSubClient mqttClient(espClient);
struct Phase { String state; unsigned long durationMs; bool buzzer; };
Phase phaseQueue[8];
int phaseCount = 0;
int currentPhase = -1;
unsigned long phaseStartedAt = 0;

void RECONNECT_MQTT() {
  while (!mqttClient.connected()) {
    Serial.print("Connecting to MQTT broker...");
    bool ok;
    if (strlen(MQTT_USER) > 0) {
      ok = mqttClient.connect(CLIENT_ID, MQTT_USER, MQTT_PASS,
                               TOPIC_C6_STATUS, 0, true, "offline");
    } else {
      ok = mqttClient.connect(CLIENT_ID, TOPIC_C6_STATUS, 0, true, "offline");
    }
    if (ok) {
      Serial.println("connected");
      mqttClient.publish(TOPIC_C6_STATUS, "online", true);
      mqttClient.subscribe(TOPIC_LIGHT_CONTROL);
    } else {
      Serial.printf("failed, rc=%d, retrying in 2s\n", mqttClient.state());
      delay(2000);
    }
  }
}

void setLeds(bool red, bool yellow, bool green) {
  digitalWrite(PIN_LED_RED, red ? HIGH : LOW);
  digitalWrite(PIN_LED_YELLOW, yellow ? HIGH : LOW);
  digitalWrite(PIN_LED_GREEN, green ? HIGH : LOW);
}

void applyPhase(const Phase& p) {
  Serial.printf("Phase -> %s for %lums (buzzer=%d)\n", p.state.c_str(), p.durationMs, p.buzzer);
  setLeds(p.state == "red", p.state == "yellow", p.state == "green");
  digitalWrite(PIN_BUZZER, p.buzzer ? HIGH : LOW);
}

void startPhaseQueue() {
  currentPhase = 0;
  phaseStartedAt = millis();
  if (phaseCount > 0) applyPhase(phaseQueue[0]);
}

void updatePhaseQueue() {
  if (currentPhase < 0 || currentPhase >= phaseCount) return;
  if (millis() - phaseStartedAt >= phaseQueue[currentPhase].durationMs) {
    currentPhase++;
    if (currentPhase < phaseCount) {
      phaseStartedAt = millis();
      applyPhase(phaseQueue[currentPhase]);
    } else {
      // Cycle finished; hold at red (safe default) until the next command.
      setLeds(true, false, false);
      digitalWrite(PIN_BUZZER, LOW);
      currentPhase = -1;
    }
  }
}

void SETUP_WIFI(){
    Serial.printf("Connecting to Wi-Fi: %s\n", WIFI_SSID);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.printf("\nWi-Fi connected, IP=%s\n", WiFi.localIP().toString().c_str());
}


void setup() {
  Serial.begin(115200);

  pinMode(PIN_LED_RED, OUTPUT);
  pinMode(PIN_LED_YELLOW, OUTPUT);
  pinMode(PIN_LED_GREEN, OUTPUT);
  pinMode(PIN_BUZZER, OUTPUT);
  setLeds(true, false, false); // default to red until first command arrives

  SETUP_WIFI();
  mqttClient.setServer(MQTT_BROKER, MQTT_PORT);
  mqttClient.setCallback(onMqttMessage);
}

void loop(){
if (!mqttClient.connected()) {
    reconnectMqtt();
  }
  mqttClient.loop();
  updatePhaseQueue();
}
