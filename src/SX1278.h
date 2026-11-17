#ifndef SX1278_H
#define SX1278_H

#include <Arduino.h>
#include <SPI.h>
#include <LoRa.h>

class SX1278 {
private:
    long frequency;
    bool initialized;
    int lastRssi;
    float lastSnr;

public:
    static const int16_t RECEIVE_NONE = 0;
    static const int16_t RECEIVE_ERROR = -1;
    static const int16_t RECEIVE_BUFFER_TOO_SMALL = -2;

    SX1278() : frequency(433E6), initialized(false), lastRssi(0), lastSnr(0.0f) {}

    bool begin(
        long loraFrequency,
        int8_t chipSelectPin,
        int8_t resetPin,
        int8_t interruptPin,
        int8_t sckPin = 19,
        int8_t misoPin = 18,
        int8_t mosiPin = 23
    ) {
        frequency = loraFrequency;
        SPI.begin(sckPin, misoPin, mosiPin);
        LoRa.setPins(chipSelectPin, resetPin, interruptPin);

        initialized = LoRa.begin(frequency);
        if (!initialized) {
            Serial.println("SX1278: no se pudo iniciar el radio LoRa.");
            return false;
        }

        LoRa.setSpreadingFactor(7);
        LoRa.setSignalBandwidth(125E3);
        LoRa.setCodingRate4(5);
        LoRa.disableCrc();
        LoRa.setPreambleLength(8);
        LoRa.setSyncWord(0x12);

        Serial.println("SX1278: radio LoRa iniciado.");
        return true;
    }

    bool send(const String& payload) {
        if (!initialized) {
            Serial.println("SX1278: el radio no esta inicializado.");
            return false;
        }

        LoRa.beginPacket();
        LoRa.print(payload);
        return LoRa.endPacket() == 1;
    }

    int16_t receive(uint8_t* buffer, size_t bufferSize) {
        if (!initialized || buffer == nullptr) {
            return RECEIVE_ERROR;
        }

        int packetSize = LoRa.parsePacket();
        if (packetSize == 0) {
            return RECEIVE_NONE;
        }

        if (static_cast<size_t>(packetSize) > bufferSize) {
            while (LoRa.available()) {
                LoRa.read();
            }
            return RECEIVE_BUFFER_TOO_SMALL;
        }

        int16_t length = 0;
        while (LoRa.available() && length < packetSize) {
            buffer[length++] = static_cast<uint8_t>(LoRa.read());
        }

        lastRssi = LoRa.packetRssi();
        lastSnr = LoRa.packetSnr();
        return length;
    }

    int packetRssi() const {
        return lastRssi;
    }

    float packetSnr() const {
        return lastSnr;
    }
};

#endif
