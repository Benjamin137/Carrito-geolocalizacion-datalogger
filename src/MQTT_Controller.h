#ifndef MQTT_CONTROLLER_H
#define MQTT_CONTROLLER_H

#include <Arduino.h>
#include <WiFi.h>
#include <functional>
#include <PubSubClient.h>

class MQTT_Controller {

public:

    // Callback al recibir un mensaje (topic y payload ya como String)
    typedef std::function<void(const String& topic, const String& payload)> MessageCallback;


private:

    // ==========================================
    // Clientes
    // ==========================================

    WiFiClient wifiClient;
    PubSubClient mqttClient;


    // ==========================================
    // Configuración MQTT
    // ==========================================

    String mqtt_server;
    int mqtt_port;

    String mqtt_user;
    String mqtt_password;

    String mqtt_topic;
    String mqtt_clientId;

    String availabilityTopic;      // topic de "online/offline" (opcional)


    // ==========================================
    // Suscripciones (se repiten tras reconectar)
    // ==========================================

    static const uint8_t MAX_SUBS = 8;

    String subs[MAX_SUBS];
    uint8_t subCount;

    MessageCallback messageCallback;


    // ==========================================
    // Reconexión no bloqueante
    // ==========================================

    unsigned long retryInterval;
    unsigned long lastAttempt;
    bool everTried;

    bool debug;


    // ==========================================
    // Conexión real al broker
    // ==========================================

    bool doConnect() {

        if (mqtt_clientId.length() == 0) {

            // ClientId único con la MAC (dos clientes con el mismo id se desconectan entre sí)
            mqtt_clientId = "esp32-" + String((uint32_t)(ESP.getEfuseMac() >> 24), HEX);
        }

        const char* will = (availabilityTopic.length() > 0)
                               ? availabilityTopic.c_str()
                               : nullptr;

        bool hasUser = (mqtt_user.length() > 0);

        bool ok = mqttClient.connect(
            mqtt_clientId.c_str(),
            hasUser ? mqtt_user.c_str() : nullptr,
            hasUser ? mqtt_password.c_str() : nullptr,
            will,          // topic del Last Will
            0,
            true,
            "offline",     // mensaje que publica el broker si el ESP32 se cae
            true
        );

        if (!ok) {

            if (debug) {

                Serial.print("MQTT no conectado: ");
                Serial.println(stateText());
            }

            return false;
        }

        if (debug) {

            Serial.println("MQTT conectado como " + mqtt_clientId);
        }

        if (will) {

            mqttClient.publish(will, "online", true);
        }

        for (uint8_t i = 0; i < subCount; i++) {

            mqttClient.subscribe(subs[i].c_str());
        }

        return true;
    }


public:

    // ==========================================
    // Constructor
    // ==========================================

    MQTT_Controller()
        : mqttClient(wifiClient),
          mqtt_port(1883),
          subCount(0),
          retryInterval(5000),
          lastAttempt(0),
          everTried(false),
          debug(true)
    {
        mqttClient.setBufferSize(512);     // por defecto son solo 256 bytes
        mqttClient.setKeepAlive(30);
        mqttClient.setSocketTimeout(5);    // evita bloqueos largos si el broker no responde

        mqttClient.setCallback(
            [this](char* topic, uint8_t* payload, unsigned int length) {

                if (!messageCallback) return;

                String msg;
                msg.reserve(length);

                for (unsigned int i = 0; i < length; i++) {

                    msg += (char)payload[i];
                }

                messageCallback(String(topic), msg);
            }
        );
    }

    // El objeto contiene referencias internas: no se puede copiar
    MQTT_Controller(const MQTT_Controller&) = delete;
    MQTT_Controller& operator=(const MQTT_Controller&) = delete;


    // ==========================================
    // Configurar servidor MQTT
    // ==========================================

    void setServer(
        String server,
        int port
    ) {

        mqtt_server = server;
        mqtt_port = port;

        mqttClient.setServer(
            mqtt_server.c_str(),
            mqtt_port
        );
    }


    // ==========================================
    // Configurar credenciales
    // ==========================================

    void setCredentials(
        String user,
        String password
    ) {

        mqtt_user = user;
        mqtt_password = password;
    }


    // ==========================================
    // Configurar tópico (el de publish(mensaje))
    // ==========================================

    void setTopic(
        String topic
    ) {

        mqtt_topic = topic;
    }


    // ==========================================
    // Opciones nuevas
    // ==========================================

    // Tamaño máximo de mensaje en bytes (por defecto 512)
    void setBufferSize(uint16_t size) {

        mqttClient.setBufferSize(size);
    }

    // Cada cuántos ms reintenta conectar al broker si se cae (por defecto 5 s)
    void setRetryInterval(unsigned long ms) {

        retryInterval = ms;
    }

    // Topic de disponibilidad: publica "online" al conectar y el broker
    // publica "offline" automáticamente si el ESP32 se desconecta.
    void setAvailabilityTopic(String topic) {

        availabilityTopic = topic;
    }

    // Activa o desactiva los mensajes por Serial
    void setDebug(bool enabled) {

        debug = enabled;
    }


    // ==========================================
    // Conectar (guarda el clientId para reconectar solo)
    // ==========================================

    bool connect(
        String clientId
    ) {

        mqtt_clientId = clientId;

        return doConnect();
    }


    // ==========================================
    // Suscribirse (se repite tras cada reconexión)
    // ==========================================

    bool subscribe(
        String topic
    ) {

        bool known = false;

        for (uint8_t i = 0; i < subCount; i++) {

            if (subs[i] == topic) known = true;
        }

        if (!known) {

            if (subCount >= MAX_SUBS) return false;

            subs[subCount++] = topic;
        }

        if (mqttClient.connected()) {

            return mqttClient.subscribe(topic.c_str());
        }

        return true;    // quedará suscrito al conectar
    }

    void setCallback(MessageCallback cb) {

        messageCallback = cb;
    }


    // ==========================================
    // Publicar mensaje en el topic configurado
    // ==========================================

    bool publish(
        const String& message
    ) {

        return publish(mqtt_topic, message, false);
    }


    // ==========================================
    // Publicar mensaje en cualquier topic
    // ==========================================

    bool publish(
        const String& topic,
        const String& message,
        bool retained = false
    ) {

        if (!mqttClient.connected()) {

            if (debug) {

                Serial.println(
                    "MQTT no conectado. No se puede publicar."
                );
            }

            return false;
        }

        if (message.length() + topic.length() + 10 > mqttClient.getBufferSize()) {

            if (debug) {

                Serial.printf(
                    "MQTT: mensaje demasiado grande (%u bytes, buffer de %u). Usa setBufferSize().\n",
                    (unsigned)message.length(),
                    (unsigned)mqttClient.getBufferSize()
                );
            }

            return false;
        }

        bool result = mqttClient.publish(
            topic.c_str(),
            message.c_str(),
            retained
        );

        if (debug) {

            if (result) {

                Serial.println(
                    "Mensaje MQTT publicado:"
                );

                Serial.println(
                    message
                );

            } else {

                Serial.println(
                    "Error al publicar MQTT."
                );
            }
        }

        return result;
    }


    // ==========================================
    // Mantener conexión (solo procesa mensajes)
    // ==========================================

    void loop() {

        mqttClient.loop();
    }


    // ==========================================
    // Mantener conexión Y reconectar solo (NO bloqueante)
    // Usa esta en lugar de loop() si quieres reconexión automática.
    // ==========================================

    void update() {

        if (WiFi.status() != WL_CONNECTED) return;

        if (!mqttClient.connected()) {

            unsigned long now = millis();

            if (everTried && now - lastAttempt < retryInterval) return;

            everTried = true;
            lastAttempt = now;

            doConnect();

            return;
        }

        mqttClient.loop();
    }


    // ==========================================
    // Estado de conexión
    // ==========================================

    bool connected() {

        return mqttClient.connected();
    }

    int state() {

        return mqttClient.state();
    }

    const char* stateText() {

        switch (mqttClient.state()) {

            case -4: return "Timeout: el broker no respondio";
            case -3: return "Conexion perdida";
            case -2: return "No se pudo conectar al broker (revisa IP y puerto)";
            case -1: return "Desconectado";
            case  0: return "Conectado";
            case  1: return "Protocolo no soportado";
            case  2: return "ClientId rechazado";
            case  3: return "Broker no disponible";
            case  4: return "Usuario o clave incorrectos";
            case  5: return "No autorizado";
            default: return "Estado desconocido";
        }
    }

};

#endif
