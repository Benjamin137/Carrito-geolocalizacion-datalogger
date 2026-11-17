#ifndef SIM800L_H
#define SIM800L_H

#define TINY_GSM_MODEM_SIM800

#include <Arduino.h>
#include <TinyGsmClient.h>

class SIM800L {
private:
    HardwareSerial& serial;
    TinyGsm modem;
    TinyGsmClient tcpClient;
    String apn;
    String user;
    String password;
    unsigned long baudRate;
    bool connected;

public:
    SIM800L(HardwareSerial& serialPort, unsigned long baud = 9600)
        : serial(serialPort),
          modem(serialPort),
          tcpClient(modem),
          baudRate(baud),
          connected(false) {}

    SIM800L(const SIM800L&) = delete;
    SIM800L& operator=(const SIM800L&) = delete;

    void setGprsCredentials(
        const String& accessPointName,
        const String& username = "",
        const String& userPassword = ""
    ) {
        apn = accessPointName;
        user = username;
        password = userPassword;
    }

    bool begin(int8_t rxPin, int8_t txPin, int8_t powerPin = -1) {
        if (powerPin >= 0) {
            pinMode(powerPin, OUTPUT);
            digitalWrite(powerPin, HIGH);
            delay(1000);
        }

        serial.begin(baudRate, SERIAL_8N1, rxPin, txPin);
        delay(300);

        if (!modem.restart()) {
            Serial.println("SIM800L: no se pudo iniciar el modem.");
            return false;
        }

        if (!modem.waitForNetwork()) {
            Serial.println("SIM800L: no se encontro la red movil.");
            return false;
        }

        if (!modem.gprsConnect(apn.c_str(), user.c_str(), password.c_str())) {
            Serial.println("SIM800L: no se pudo conectar a GPRS.");
            return false;
        }

        connected = true;
        Serial.println("SIM800L: GPRS conectado.");
        return true;
    }

    void update() {
        if (connected && !modem.isGprsConnected()) {
            connected = false;
        }
    }

    bool isConnected() {
        return connected && modem.isGprsConnected();
    }

    TinyGsmClient& client() {
        return tcpClient;
    }
};

#endif
