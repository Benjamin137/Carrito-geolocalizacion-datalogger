#ifndef SX1278_H
#define SX1278_H

#include <Arduino.h>
#include <LoRa.h>

class SX1278 {
private:
    long frequency;
    bool initialized;

public:
    SX1278() : frequency(433E6), initialized(false) {}

    bool begin(
        long loraFrequency,
        int8_t chipSelectPin,
        int8_t resetPin,
        int8_t interruptPin
    ) {
        frequency = loraFrequency;
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
};

#endif
