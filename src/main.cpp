#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <WiFiManager.h>
#include <PubSubClient.h>
#include "HX711.h"

/* MQTT credentials */
const char *MQTT_HOST = "p9869662.ala.asia-southeast1.emqxsl.com";
constexpr int MQTT_PORT = 8883;
const char *MQTT_USER = "user1";
const char *MQTT_PASS = "Pass1234";

/* MQTT topic */
const char *MQTT_TOPIC = "test/topic";

/* Root CA certificate */
const char *ROOT_CA = R"EOF(
-----BEGIN CERTIFICATE-----
MIIDjjCCAnagAwIBAgIQAzrx5qcRqaC7KGSxHQn65TANBgkqhkiG9w0BAQsFADBh
MQswCQYDVQQGEwJVUzEVMBMGA1UEChMMRGlnaUNlcnQgSW5jMRkwFwYDVQQLExB3
d3cuZGlnaWNlcnQuY29tMSAwHgYDVQQDExdEaWdpQ2VydCBHbG9iYWwgUm9vdCBH
MjAeFw0xMzA4MDExMjAwMDBaFw0zODAxMTUxMjAwMDBaMGExCzAJBgNVBAYTAlVT
MRUwEwYDVQQKEwxEaWdpQ2VydCBJbmMxGTAXBgNVBAsTEHd3dy5kaWdpY2VydC5j
b20xIDAeBgNVBAMTF0RpZ2lDZXJ0IEdsb2JhbCBSb290IEcyMIIBIjANBgkqhkiG
9w0BAQEFAAOCAQ8AMIIBCgKCAQEAuzfNNNx7a8myaJCtSnX/RrohCgiN9RlUyfuI
2/Ou8jqJkTx65qsGGmvPrC3oXgkkRLpimn7Wo6h+4FR1IAWsULecYxpsMNzaHxmx
1x7e/dfgy5SDN67sH0NO3Xss0r0upS/kqbitOtSZpLYl6ZtrAGCSYP9PIUkY92eQ
q2EGnI/yuum06ZIya7XzV+hdG82MHauVBJVJ8zUtluNJbd134/tJS7SsVQepj5Wz
tCO7TG1F8PapspUwtP1MVYwnSlcUfIKdzXOS0xZKBgyMUNGPHgm+F6HmIcr9g+UQ
vIOlCsRnKPZzFBQ9RnbDhxSJITRNrw9FDKZJobq7nMWxM4MphQIDAQABo0IwQDAP
BgNVHRMBAf8EBTADAQH/MA4GA1UdDwEB/wQEAwIBhjAdBgNVHQ4EFgQUTiJUIBiV
5uNu5g/6+rkS7QYXjzkwDQYJKoZIhvcNAQELBQADggEBAGBnKJRvDkhj6zHd6mcY
1Yl9PMWLSn/pvtsrF9+wX3N3KjITOYFnQoQj8kVnNeyIv/iPsGEMNKSuIEyExtv4
NeF22d+mQrvHRAiGfzZ0JFrabA0UWTW98kndth/Jsw1HKj2ZL7tcu7XUIOGZX1NG
Fdtom/DzMNU+MeKNhJ7jitralj41E6Vf8PlwUHBHQRFXGU7Aj64GxJUTFy8bJZ91
8rGOmaFvE7FBcf6IKshPECBV1/MUReXgRPTqh5Uykw7+U0b6LJ3/iyK5S9kJRaTe
pLiaWN0bfVKfjllDiIGknibVb63dDcY3fe0Dkhvld1927jyNxF1WW6LZZm6zNTfl
MrY=
-----END CERTIFICATE-----
)EOF";

/* Heartbeat time */
unsigned long lastHeartbeatTime = 0;
constexpr unsigned long HEARTBEAT_INTERVAL = 10000;

/* Platforms */
typedef enum {
    DISABLE,
    IDLE,
    EMPTY
} PlatformState;

PlatformState p_states[2];
HX711 scales[2];

constexpr int LOADCELL_1_DOUT_PIN = 12;
constexpr int LOADCELL_1_SCK_PIN = 14;

constexpr int LOADCELL_2_DOUT_PIN = 27;
constexpr int LOADCELL_2_SCK_PIN = 26;

unsigned long emptyStartTime[2];
unsigned long lastEmptyWarnTime[2];
constexpr unsigned long EMPTY_WARN_TIME_THRESHOLD = 20;
constexpr unsigned long EMPTY_TIME_MAX = 100;

constexpr float WEIGHT_THRESHOLD = 50.0;
constexpr unsigned long READ_INTERVALS = 100;

WiFiClientSecure secureClient;
PubSubClient mqttClient(secureClient);

void connectMQTT() {
    while (!mqttClient.connected()) {
        Serial.print("Connecting to MQTT... ");

        if (mqttClient.connect("ESP32Client", MQTT_USER, MQTT_PASS)) {
            Serial.println("connected");
        } else {
            Serial.print("failed, rc=");
            Serial.print(mqttClient.state());
            Serial.println(" retrying in 5s");
            delay(5000);
        }
    }
}

void publishData(String message) {
    if (mqttClient.publish(MQTT_TOPIC, message.c_str())) {
        Serial.println("Published: " + message);
    } else {
        Serial.println("Failed to publish message");
    }
}

void sendHeartbeat() {
    unsigned long currentMillis = millis();
    if (currentMillis - lastHeartbeatTime >= HEARTBEAT_INTERVAL) {
        lastHeartbeatTime = currentMillis;
        String message = "Heartbeat, time: " + String(currentMillis / 1000);
        publishData(message);
    }
}


void scaleLoop(int i) {
    float weight;
    unsigned long currentMillis;
    unsigned long emptyTime;
    switch (p_states[i]) {
        case DISABLE:
            weight = scales[i].get_units(5);
            if (weight > WEIGHT_THRESHOLD) {
                p_states[i] = IDLE;
                Serial.printf("Activate, Platform %d", i);
            }
            break;
        case IDLE:
            weight = scales[i].get_units(10);
            if (weight < WEIGHT_THRESHOLD) {
                p_states[i] = EMPTY;
                emptyStartTime[i] = millis();
                Serial.printf("Put up, Platform %d", i);
                publishData("p" + String(i));
            }
            break;
        case EMPTY:
            weight = scales[i].get_units(5);
            if (weight > WEIGHT_THRESHOLD) {
                p_states[i] = IDLE;
                Serial.printf("Put back, Platform %d", i);
            } else {
                currentMillis = millis();
                emptyTime = currentMillis - emptyStartTime[i];
                if (emptyTime >= EMPTY_WARN_TIME_THRESHOLD) {
                    if (currentMillis - lastEmptyWarnTime[i] >= EMPTY_WARN_TIME_THRESHOLD) {
                        Serial.printf("Warning, Platform %d is empty for %lu seconds", i, emptyTime / 1000);
                        publishData("w" + String(i));
                    }
                    if (emptyTime > EMPTY_TIME_MAX) {
                        p_states[i] = DISABLE;
                        Serial.printf("Disable, Platform %d", i);
                        publishData("d" + String(i));
                    }
                }
            }
            break;
    }
}

void setup() {
    Serial.begin(115200);
    delay(1000);

    WiFiManager wifiManager;
    if (!wifiManager.autoConnect("ESP32_AP_Pill")) {
        Serial.println("Failed to connect or configure WiFi, restarting...");
        delay(3000);
        ESP.restart();
    }
    Serial.println("WiFi connected: " + WiFi.localIP().toString());

    // TLS and MQTT
    secureClient.setCACert(ROOT_CA);
    mqttClient.setServer(MQTT_HOST, MQTT_PORT);
    connectMQTT();

    // scale initialization
    Serial.println("Initializing scales...");
    scales[0].begin(LOADCELL_1_DOUT_PIN, LOADCELL_1_SCK_PIN);
    scales[1].begin(LOADCELL_2_DOUT_PIN, LOADCELL_2_SCK_PIN);
    delay(2000);
    for (int i = 0; i < 2; i++) {
        scales[i].set_scale(1.0);
        scales[i].tare();
        p_states[0] = DISABLE;
    }
    Serial.println("Initialized scales");
}

void loop() {
    if (!mqttClient.connected()) {
        connectMQTT();
    }
    mqttClient.loop();
    sendHeartbeat();
    scaleLoop(0);
    scaleLoop(1);
}
