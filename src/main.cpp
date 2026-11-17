#include <Arduino.h>
#include <WiFi.h>

#include "MQTT_Controller.h"
#include "SIM800L.h"
#include "SX1278.h"
#include "Wifi_Connect.h"

// Se lee una sola vez durante setup(). Cambiar a false para usar SIM800L.
const bool USE_WIFI = true;
const bool USE_LORA = true;

const char* WIFI_SSID = "INFINITUM4DA7";
const char* WIFI_PASSWORD = "ePrRukdZyK";

const char* SIM_APN = "TU_APN";
const char* SIM_USER = "";
const char* SIM_PASSWORD = "";

const char* MQTT_SERVER = "test.mosquitto.org";
const int MQTT_PORT = 1883;
const char* MQTT_CLIENT_ID = "gateway-carrito";
const char* MQTT_SUBSCRIBE_TOPIC = "carrito/datos";
const char* MQTT_AVAILABILITY_TOPIC = "carrito/gateway/estado";

constexpr int SIM_RX_PIN = 16;
constexpr int SIM_TX_PIN = 17;
constexpr int LORA_CS_PIN = 5;
constexpr int LORA_RESET_PIN = -1;
constexpr int LORA_IRQ_PIN = 26;
constexpr long LORA_FREQUENCY = 433E6;
constexpr int STATUS_LED_PIN = 2;
constexpr unsigned long STATUS_LED_DURATION_MS = 150;

HardwareSerial simSerial(1);
SIM800L sim800l(simSerial);
SX1278 sx1278;
Wifi_Connect wifi;
WiFiClient wifiClient;
MQTT_Controller wifiMqtt(wifiClient, []() {
    return WiFi.status() == WL_CONNECTED;
});
MQTT_Controller simMqtt(sim800l.client(), []() {
    return sim800l.isConnected();
});
MQTT_Controller* mqtt = nullptr;
unsigned long statusLedOffAt = 0;

void blinkStatusLed() {
    digitalWrite(STATUS_LED_PIN, HIGH);
    statusLedOffAt = millis() + STATUS_LED_DURATION_MS;
}

void updateStatusLed() {
    if (statusLedOffAt != 0 &&
        static_cast<long>(millis() - statusLedOffAt) >= 0) {
        digitalWrite(STATUS_LED_PIN, LOW);
        statusLedOffAt = 0;
    }
}

void onMqttMessage(const String& topic, const String& payload) {

    blinkStatusLed();

    if (!USE_LORA) {
        return;
    }

    if (!sx1278.send(payload)) {
        Serial.println("No se pudo enviar el mensaje por LoRa.");
    } else {
        
        Serial.print("Mensaje enviado por LoRa: ");
        Serial.println(payload);
    }
}

void setup() {
    Serial.begin(115200);
    delay(500);
    pinMode(STATUS_LED_PIN, OUTPUT);
    digitalWrite(STATUS_LED_PIN, LOW);

    if (USE_WIFI) {
        wifi.setCredentials(WIFI_SSID, WIFI_PASSWORD);
        wifi.startConnect();
        mqtt = &wifiMqtt;
    } else {
        sim800l.setGprsCredentials(SIM_APN, SIM_USER, SIM_PASSWORD);
        sim800l.begin(SIM_RX_PIN, SIM_TX_PIN);
        mqtt = &simMqtt;
    }

    if (USE_LORA) {
        sx1278.begin(
            LORA_FREQUENCY,
            LORA_CS_PIN,
            LORA_RESET_PIN,
            LORA_IRQ_PIN
        );
    }

    mqtt->setServer(MQTT_SERVER, MQTT_PORT);
    mqtt->setTopic(MQTT_SUBSCRIBE_TOPIC);
    mqtt->setAvailabilityTopic(MQTT_AVAILABILITY_TOPIC);
    mqtt->setCallback(onMqttMessage);
    mqtt->subscribe(MQTT_SUBSCRIBE_TOPIC);
    mqtt->connect(MQTT_CLIENT_ID);
}

void loop() {
    if (USE_WIFI) {
        wifi.update();
    } else {
        sim800l.update();
    }

    mqtt->update();
    updateStatusLed();
}
