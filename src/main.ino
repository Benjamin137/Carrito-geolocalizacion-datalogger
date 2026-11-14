#include "DRV8833.h"
#include "carrito.h"
#include "Oled.h"
#include "RTCClock.h"
#include "SDModule.h"
#include "AHTBMP280Sensor.h"
#include "TCRT5000Sensor.h"
#include "HCSR04Sensor.h"
#include "SX1278.h"
#include <ArduinoJson.h>

const int SDA_PIN = 21;
const int SCL_PIN = 22;
const uint8_t TCRT5000_PIN = 34;
const uint8_t TCRT5000_PINS[] = {TCRT5000_PIN};
const uint8_t HCSR04_TRIGGER_PIN = 4;
const uint8_t HCSR04_ECHO_PIN = 35;
const float DISTANCIA_MINIMA_CM = 20.0f;
const long SX1278_FREQUENCY = 433000000L;
const uint8_t SX1278_CS_PIN = 17;
const uint8_t SX1278_RESET_PIN = 16;
const int8_t SX1278_DIO0_PIN = 2;

OLEDI2C oled(0x3C, 128, 64, 2, SDA_PIN, SCL_PIN);
RTCClock rtc(SDA_PIN, SCL_PIN, RTCClock::LANG_ES);
SDModule sd;
// AHTBMP280Sensor ahtbmp(0x38, 0x76, SDA_PIN, SCL_PIN);
TCRT5000Sensor tcrt5000(TCRT5000_PINS, 1, true);
HCSR04Sensor hcsr04(HCSR04_TRIGGER_PIN, HCSR04_ECHO_PIN);
SX1278 sx1278(SX1278_CS_PIN, SX1278_RESET_PIN, SX1278_DIO0_PIN);

// Llanta trasera izquierda
// BOUT1: rojo; BOUT2: negro

// Llanta trasera derecha
// AOUT1: rojo; AOUT2: negro

DRV8833 ladoIzquierdo(
    32, // AIN1
    33, // AIN2
    25, // BIN1
    26, // BIN2
    false, false, 20000, 8, 0);

// Llanta trasera derecha
// AOUT1: rojo; AOUT2: negro

// Llanta delantera derecha
// BOUT1: rojo; BOUT2: negro
DRV8833 ladoDerecho(
    14, // AIN1
    27, // AIN2
    15, // BIN1
    13, // BIN2
    false, false, 20000, 8, 4);

Carrito carrito(ladoIzquierdo, ladoDerecho);

bool rtcAvailable = false;
bool carritoAvailable = false;
bool oledAvailable = false;
bool sdAvailable = false;
// bool ahtbmpAvailable = false;
bool tcrt5000Available = false;
bool lastLineDetected = false;
bool hcsr04Available = false;
bool obstacleDetected = false;
bool sx1278Available = false;
bool buscandoLineaDespuesDeObstaculo = false;
bool avanceManualActivo = false;
bool carritoEnMovimiento = false;
// Activar para probar las instrucciones escribiendolas en el monitor serial.
bool modoPruebaTerminal = true;

unsigned long lastDisplayUpdate = 0;
unsigned long lastLogUpdate = 0;
unsigned long lastDistanceRead = 0;

const char *ACTION_LOG = "/acciones.log";
const char *LOCATION_LOG = "/ubicacion.log";
const char *ENVIRONMENT_LOG = "/ambiente.log";

enum ModoOperacion
{
    MODO_MANUAL,
    MODO_AUTOMATICO
};

ModoOperacion modoActual = MODO_MANUAL;

// Duración total de la prueba: 4 fases x (2300 ms movimiento + 200 ms pausa) = 10 s
const unsigned long DURACION_FASE_MS = 2300;
const unsigned long PAUSA_ENTRE_FASES_MS = 200;
const int VELOCIDAD_PRUEBA = 200;

// Declaraciones adelantadas
void pruebaMotores();
void ejecutarFasePrueba(const char *nombre, const char *evento, void (Carrito::*accion)(int));
void updateDisplay();
void logStatus();
void logEvent(const char *path, const String &event);
String timestamp();
const char *modeName();
String getInstruction();
bool evitarObstaculo();

void setup()
{
    Serial.begin(115200);

    String message;

    carritoAvailable = carrito.begin();
    sdAvailable = sd.begin();
    oledAvailable = oled.begin();
    delay(2000);
    rtcAvailable = rtc.begin();
    // ahtbmpAvailable = ahtbmp.begin();
    tcrt5000Available = tcrt5000.begin();
    hcsr04.begin();
    hcsr04Available = true;
    sx1278Available = sx1278.begin(SX1278_FREQUENCY);
    message = "";

    if (!oledAvailable)
    {
        message += "Error: no se encontro la pantalla OLED\n";
    }

    if (!carritoAvailable)
    {
        message += "Error: no se inicializo el carrito\n";
    }

    if (!rtcAvailable)
    {
        message += "Error: no se encontro el RTC\n";
    }

    if (!tcrt5000Available)
    {
        message += "Error: no se inicializo el TCRT5000\n";
    }

    if (!hcsr04Available)
    {
        message += "Error: no se inicializo el HC-SR04\n";
    }

    if (!sx1278Available)
    {
        message += "Error: no se inicializo el SX1278\n";
    }

    // if (!ahtbmpAvailable) {
    //     message += "Error: no se inicializo el sensor AHT/BMP280\n";
    // }

    // if (!sdAvailable) {
    //     message += "Error: no se inicializo la tarjeta SD\n";
    // }

    if (message.length() > 0)
    {
        if (oledAvailable)
            oled.showDanger(1, message.c_str());
        return;
    }
    pruebaMotores();
}

void loop()
{
    String instruction;
    if (modoActual != MODO_AUTOMATICO || modoPruebaTerminal)
    {
        instruction = getInstruction();
    }

    evitarObstaculo();

    if (instruction.length() > 0)
    {
        Serial.print("Instruccion recibida: ");
        Serial.println(instruction);

        if (instruction == "W")
        {
            if (!obstacleDetected)
            {
                carrito.avanzar(180);
                avanceManualActivo = true;
                carritoEnMovimiento = true;
            }
            else
            {
                carrito.detener();
                avanceManualActivo = false;
                carritoEnMovimiento = false;
                Serial.println("Movimiento W ignorado: obstaculo detectado");
            }
        }
        else if (instruction == "A")
        {
            carrito.girarIzquierda(180);
            avanceManualActivo = false;
            carritoEnMovimiento = true;
        }
        else if (instruction == "S")
        {
            carrito.reversa(180);
            avanceManualActivo = false;
            carritoEnMovimiento = true;
        }
        else if (instruction == "D")
        {
            carrito.girarDerecha(180);
            avanceManualActivo = false;
            carritoEnMovimiento = true;
        }
        else if (instruction == "stop")
        {
            carrito.detener();
            avanceManualActivo = false;
            carritoEnMovimiento = false;
        }
    }
    else if (modoActual == MODO_AUTOMATICO && tcrt5000Available)
    {
        bool lineDetected = tcrt5000.isLineDetected(0);
        lastLineDetected = lineDetected;

        if (obstacleDetected || buscandoLineaDespuesDeObstaculo)
        {
            if (obstacleDetected || !lineDetected)
            {
                buscandoLineaDespuesDeObstaculo = true;
                carrito.girarDerecha(180);
                carritoEnMovimiento = true;
            }
            else
            {
                buscandoLineaDespuesDeObstaculo = false;
                carrito.avanzar(180);
                carritoEnMovimiento = true;
            }
        }
        else if (lineDetected)
        {
            carrito.avanzar(180);
            carritoEnMovimiento = true;
        }
        else
        {
            carrito.detener();
            carritoEnMovimiento = false;
        }
    }

    if (rtcAvailable && oledAvailable){
        unsigned long now = millis();
        if (now - lastDisplayUpdate >= 1000)
        {
            lastDisplayUpdate = now;
            updateDisplay();
        }
    }

    unsigned long logInterval = carritoEnMovimiento ? 3000 : 10000;
    unsigned long now = millis();
    if (sdAvailable && now - lastLogUpdate >= logInterval)
    {
        lastLogUpdate = now;
        logStatus();
    }
}

// ---------------------------------------------------------------------------
// Prueba de motores (solo se llama desde setup)
// ---------------------------------------------------------------------------

void pruebaMotores()
{
    if (!carritoAvailable)
        return;

    modoActual = MODO_AUTOMATICO;

    carrito.avanzar(200);
    delay(5000);
    carrito.detener();
    delay(3000);
    carrito.reversa(200);
    delay(5000);
    carrito.detener();
    delay(3000);
    carrito.girarDerecha(200);
    delay(5000);
    carrito.detener();
    delay(3000);
    carrito.girarIzquierda(200);
    delay(5000);
    carrito.detener();
}

// ---------------------------------------------------------------------------
// Pantalla y registros
// ---------------------------------------------------------------------------

void updateDisplay()
{
    oled.printCentered(0, rtc.getTime24());
    oled.printCentered(2, rtc.getDateLatin());
    oled.printCentered(3, rtc.getDayName(true));
}

void logStatus()
{
    if (!rtcAvailable || !sdAvailable)
        return;

    // bool sensorUpdated = ahtbmpAvailable && ahtbmp.update();
    String line = timestamp();
    line += ",carrito=";
    line += carritoAvailable ? "listo" : "error";
    line += ",modo=";
    line += modeName();
    // line += ",temperatura=";
    // if (sensorUpdated && ahtbmp.hasAHT())
    //     line += String(ahtbmp.getTemperature(), 2);
    // else
        // line += "na";
    // line += ",humedad=";
    // if (sensorUpdated && ahtbmp.hasAHT())
        // line += String(ahtbmp.getHumidity(), 2);
    // else
        // line += "na";
    // line += ",presion_hpa=";
    // if (sensorUpdated && ahtbmp.hasBMP())
        // line += String(ahtbmp.getPressureHpa(), 2);
    // else
        // line += "na";
    sd.appendLine(ACTION_LOG, line);
}

bool evitarObstaculo()
{
    if (!hcsr04Available)
        return false;

    unsigned long now = millis();
    if (now - lastDistanceRead < 100)
        return obstacleDetected;

    lastDistanceRead = now;
    float distanceCm = hcsr04.readCentimeters();
    bool newObstacleDetected =
        !isnan(distanceCm) && distanceCm <= DISTANCIA_MINIMA_CM;

    if (newObstacleDetected != obstacleDetected)
    {
        obstacleDetected = newObstacleDetected;
        Serial.println(obstacleDetected
                           ? "HC-SR04: obstaculo detectado"
                           : "HC-SR04: camino libre");
    }

    if (obstacleDetected && modoActual == MODO_AUTOMATICO)
    {
        carrito.detener();
        buscandoLineaDespuesDeObstaculo = true;
    }
    else if (obstacleDetected && avanceManualActivo)
    {
        carrito.detener();
        avanceManualActivo = false;
        carritoEnMovimiento = false;
    }

    return obstacleDetected;
}

String getInstruction()
{
    if (modoPruebaTerminal)
    {
        static String terminalInstruction;

        while (Serial.available() > 0)
        {
            char character = static_cast<char>(Serial.read());
            if (character == '\n' || character == '\r')
            {
                if (terminalInstruction.length() == 0)
                    continue;

                String command = terminalInstruction;
                terminalInstruction = "";
                command.trim();
                if (command == "W" || command == "A" || command == "S" ||
                    command == "D" || command == "stop")
                    return command;

                Serial.printf("Instruccion no valida: %s\n", command.c_str());
                return "";
            }

            if (terminalInstruction.length() < 32)
                terminalInstruction += character;
        }

        return "";
    }

    if (!sx1278Available)
        return "";

    uint8_t payload[256];
    int16_t length = sx1278.receive(payload, sizeof(payload) - 1);
    if (length == SX1278::RECEIVE_NONE)
        return "";

    if (length < 0)
    {
        Serial.printf("SX1278: error al recibir paquete (%d)\n", length);
        return "";
    }

    payload[length] = '\0';
    JsonDocument document;
    DeserializationError error = deserializeJson(document, payload, length);
    if (error)
    {
        Serial.printf("SX1278: JSON invalido: %s\n", error.c_str());
        return "";
    }

    JsonVariant instruction = document["goto"];
    if (instruction.isNull())
    {
        Serial.println("SX1278: el JSON no contiene la propiedad goto");
        return "";
    }

    String command = instruction.as<String>();
    if (command != "W" && command != "A" && command != "S" &&
        command != "D" && command != "stop")
    {
        Serial.printf("SX1278: movimiento no valido: %s\n", command.c_str());
        return "";
    }

    return command;
}

void logEvent(const char *path, const String &event)
{
    if (!sdAvailable)
        return;

    String line = rtcAvailable ? timestamp() : "sin_rtc";
    line += ",";
    line += event;
    sd.appendLine(path, line);
}

String timestamp()
{
    return rtc.getDateTimeLatin();
}

const char *modeName()
{
    switch (modoActual)
    {
    case MODO_MANUAL:
        return "manual";
    case MODO_AUTOMATICO:
        return "automatico";
    default:
        return "desconocido";
    }
}
