#ifndef WIFI_CONNECT_H
#define WIFI_CONNECT_H

#include <Arduino.h>
#include <WiFi.h>

class Wifi_Connect {

private:

    String ssid;
    String password;

    unsigned long retryInterval = 15000;   // cada cuánto reintenta si se cae
    unsigned long lastAttempt = 0;


public:

    // ==========================================
    // Setter para configurar las credenciales
    // ==========================================

    void setCredentials(const String& ssid, const String& password) {

        this->ssid = ssid;
        this->password = password;
    }


    // ==========================================
    // Cada cuántos ms reintenta si el WiFi se cae
    // ==========================================

    void setRetryInterval(unsigned long ms) {

        retryInterval = ms;
    }


    // ==========================================
    // Conectar a WiFi (bloqueante, con tiempo límite)
    // ==========================================

    bool connect(unsigned long timeoutMs = 10000) {

        // Si ya está conectado a esa red, no hace falta reconectar
        if (WiFi.status() == WL_CONNECTED && WiFi.SSID() == ssid) {

            return true;
        }

        WiFi.mode(WIFI_STA);

        WiFi.persistent(false);          // no escribe las credenciales en la flash

        WiFi.setAutoReconnect(true);

        WiFi.disconnect();

        delay(100);

        Serial.print("Conectando a red 2.4 GHz: ");
        Serial.println(ssid);

        WiFi.begin(
            ssid.c_str(),
            password.c_str()
        );

        unsigned long start = millis();

        while (
            WiFi.status() != WL_CONNECTED &&
            millis() - start < timeoutMs
        ) {

            delay(500);

            Serial.print(".");
        }

        Serial.println();


        if (WiFi.status() == WL_CONNECTED) {

            Serial.println("¡WiFi conectado!");

            Serial.print("IP asignada: ");

            Serial.println(
                WiFi.localIP()
            );

            return true;
        }


        Serial.print("Error de conexion: ");

        Serial.println(statusText());

        return false;
    }


    // ==========================================
    // Iniciar la conexión SIN esperar (NO bloqueante)
    // Después revisa isConnected() para saber si ya conectó
    // ==========================================

    void startConnect() {

        WiFi.mode(WIFI_STA);

        WiFi.persistent(false);

        WiFi.setAutoReconnect(true);

        WiFi.disconnect();

        delay(100);

        Serial.print("Conectando a red 2.4 GHz: ");
        Serial.println(ssid);

        WiFi.begin(
            ssid.c_str(),
            password.c_str()
        );

        lastAttempt = millis();
    }


    // ==========================================
    // Mantener la conexión (NO bloqueante)
    // Llamar en cada vuelta del loop()
    // ==========================================

    void update() {

        if (WiFi.status() == WL_CONNECTED) return;

        if (ssid.length() == 0) return;

        unsigned long now = millis();

        if (now - lastAttempt < retryInterval) return;

        lastAttempt = now;

        Serial.println("WiFi caido. Reintentando...");

        WiFi.disconnect();

        WiFi.begin(
            ssid.c_str(),
            password.c_str()
        );
    }


    // ==========================================
    // Estado
    // ==========================================

    bool isConnected() {

        return WiFi.status() == WL_CONNECTED;
    }

    String getIP() {

        return WiFi.localIP().toString();
    }

    int getRSSI() {

        return WiFi.RSSI();    // intensidad de señal en dBm (más cerca de 0 = mejor)
    }

    // Texto con el estado / motivo del fallo
    const char* statusText() {

        switch (WiFi.status()) {

            case WL_CONNECTED:       return "Conectado";
            case WL_NO_SSID_AVAIL:   return "Red no encontrada (revisa el nombre y que sea 2.4 GHz)";
            case WL_CONNECT_FAILED:  return "Fallo al conectar (revisa la contrasena)";
            case WL_CONNECTION_LOST: return "Conexion perdida";
            case WL_DISCONNECTED:    return "Desconectado (revisa nombre, contrasena y senal)";
            case WL_IDLE_STATUS:     return "Esperando conexion";
            default:                 return "Estado desconocido";
        }
    }
};

#endif
