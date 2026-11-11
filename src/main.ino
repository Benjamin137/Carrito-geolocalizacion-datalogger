#include "DRV8833.h"
#include "carrito.h"
#include "Oled.h"
#include "RTCClock.h"
#include "SDModule.h"
#include "AHTBMP280Sensor.h"
#include "TCRT5000Sensor.h"
#include "HCSR04Sensor.h"

const int SDA_PIN = 21;
const int SCL_PIN = 22;
const uint8_t TCRT5000_PIN = 34;
const uint8_t TCRT5000_PINS[] = {TCRT5000_PIN};
const uint8_t HCSR04_TRIGGER_PIN = 4;
const uint8_t HCSR04_ECHO_PIN = 35;
const float DISTANCIA_MINIMA_CM = 20.0f;

OLEDI2C oled(0x3C, 128, 64, 2, SDA_PIN, SCL_PIN);
RTCClock rtc(SDA_PIN, SCL_PIN, RTCClock::LANG_ES);
SDModule sd;
//AHTBMP280Sensor ahtbmp(0x38, 0x76, SDA_PIN, SCL_PIN);
TCRT5000Sensor tcrt5000(TCRT5000_PINS, 1, true);
HCSR04Sensor hcsr04(HCSR04_TRIGGER_PIN, HCSR04_ECHO_PIN);
DRV8833 ladoIzquierdo(
    32,  // AIN1 -> AOUT1: rojo; AOUT2: negro
    33,  // AIN2 -> AOUT1: negro; AOUT2: rojo
    25,  // BIN1 -> BOUT1: rojo; BOUT2: negro
    26,  // BIN2 -> BOUT2: negro; BOUT2: rojo
    false, false, 20000, 8, 0
);

DRV8833 ladoDerecho(
    14,  // AIN1 -> AOUT1: rojo; AOUT2: negro
    27,  // AIN2 -> AOUT1: negro; AOUT2: rojo
    15,  // BIN1 -> BOUT1: rojo; BOUT2: negro
    13,  // BIN2 -> BOUT2: negro; BOUT2: rojo
    false, false, 20000, 8, 4
);

Carrito carrito(ladoIzquierdo, ladoDerecho);

bool rtcAvailable = false;
bool carritoAvailable = false;
bool oledAvailable = false;
bool sdAvailable = false;
//bool ahtbmpAvailable = false;
bool tcrt5000Available = false;
bool lastLineDetected = false;
bool hcsr04Available = false;
bool obstacleDetected = false;

unsigned long lastDisplayUpdate = 0;
unsigned long lastLogUpdate = 0;
unsigned long lastDistanceRead = 0;

const char* ACTION_LOG = "/acciones.log";
const char* LOCATION_LOG = "/ubicacion.log";
const char* ENVIRONMENT_LOG = "/ambiente.log";

enum ModoOperacion {
    MODO_MANUAL,
    MODO_AUTOMATICO,
    MODO_SEGUIDOR_LINEA,
    MODO_SEGURO
};

ModoOperacion modoActual = MODO_SEGURO;

// Duración total de la prueba: 4 fases x (2300 ms movimiento + 200 ms pausa) = 10 s
const unsigned long DURACION_FASE_MS = 2300;
const unsigned long PAUSA_ENTRE_FASES_MS = 200;
const int VELOCIDAD_PRUEBA = 200;

// Declaraciones adelantadas
void pruebaMotores();
void ejecutarFasePrueba(const char* nombre, const char* evento, void (Carrito::*accion)(int));
void updateDisplay();
void logStatus();
void logEvent(const char* path, const String& event);
String timestamp();
const char* modeName();

void setup() {
    Serial.begin(115200);

    String message;

    carritoAvailable = carrito.begin();
    // sdAvailable = sd.begin();
    oledAvailable = oled.begin();
    delay(2000);
    rtcAvailable = rtc.begin();
    //ahtbmpAvailable = ahtbmp.begin();
    tcrt5000Available = tcrt5000.begin();
    hcsr04.begin();
    hcsr04Available = true;
    message = "";

    if (!oledAvailable) {
        message += "Error: no se encontro la pantalla OLED\n";
    }

    if (!carritoAvailable) {
        message += "Error: no se inicializo el carrito\n";
    }

    if (!rtcAvailable) {
        message += "Error: no se encontro el RTC\n";
    }

    if (!tcrt5000Available) {
        message += "Error: no se inicializo el TCRT5000\n";
    }

    if (!hcsr04Available) {
        message += "Error: no se inicializo el HC-SR04\n";
    }

    //if (!ahtbmpAvailable) {
    //    message += "Error: no se inicializo el sensor AHT/BMP280\n";
    //}

    // if (!sdAvailable) {
    //     message += "Error: no se inicializo la tarjeta SD\n";
    // }

    if (message.length() > 0) {
        Serial.println(message);
        if (oledAvailable) oled.showDanger(1, message.c_str());
        return;
    }

    // carrito.avanzar(200);
    // delay(5000);
    // carrito.detener();
    // delay(3000);
    // carrito.reversa(200);
    // delay(5000);
    // carrito.detener();
    // delay(3000);
    // carrito.girarDerecha(200);
    // delay(5000);
    // carrito.detener();
    // delay(3000);
    // carrito.girarIzquierda(200);
    // delay(5000);
    // carrito.detener();
}

void loop() {
    if (rtcAvailable && oledAvailable) {
        unsigned long now = millis();
        if (now - lastDisplayUpdate >= 1000) {
            lastDisplayUpdate = now;
            updateDisplay();
        }
    }

    if (hcsr04Available && millis() - lastDistanceRead >= 100) {
        lastDistanceRead = millis();
        float distanceCm = hcsr04.readCentimeters();
        bool validDistance = !isnan(distanceCm);
        bool newObstacleDetected = validDistance && distanceCm <= DISTANCIA_MINIMA_CM;

        if (newObstacleDetected != obstacleDetected) {
            obstacleDetected = newObstacleDetected;
            Serial.println(obstacleDetected
                ? "HC-SR04: obstaculo detectado"
                : "HC-SR04: camino libre");
        }
    }

    if (tcrt5000Available) {
        bool lineDetected = tcrt5000.isLineDetected(0);
        lastLineDetected = lineDetected;

        if (obstacleDetected) {
            carrito.detener();
        } else if (lineDetected) {
            carrito.avanzar(180);
        } else {
            carrito.detener();
        }
    }

    // if (ahtbmpAvailable) {
    //     if (ahtbmp.update()) {
    //         float temperature = ahtbmp.getTemperature();
    //         float humidity = ahtbmp.getHumidity();
    //         float pressure = ahtbmp.getPressurePa();

    //         String line = timestamp();
    //         line += ",temp=";
    //         line += temperature;
    //         line += ",hum=";
    //         line += humidity;
    //         line += ",pres=";
    //         line += pressure;

    //         if (sdAvailable) {
    //             sd.appendLine(ENVIRONMENT_LOG, line);
    //         }

    //         Serial.println(line);
    //     }
    // }

    // if (sdAvailable && millis() - lastLogUpdate >= 60000) {
    //     lastLogUpdate = millis();
    //     logStatus();
    // }
}

// ---------------------------------------------------------------------------
// Prueba de motores (solo se llama desde setup)
// ---------------------------------------------------------------------------

void ejecutarFasePrueba(const char* nombre, const char* evento, void (Carrito::*accion)(int)) {
    if (oledAvailable) oled.showMessage(nombre, rtc.getTime24());
    logEvent(ACTION_LOG, evento);

    (carrito.*accion)(VELOCIDAD_PRUEBA);
    delay(DURACION_FASE_MS);

    carrito.detener();
    delay(PAUSA_ENTRE_FASES_MS);
}

void pruebaMotores() {
    if (!carritoAvailable) return;

    modoActual = MODO_AUTOMATICO;

    ejecutarFasePrueba("PRUEBA: AVANZAR",  "prueba_avanzar",  static_cast<void (Carrito::*)(int)>(&Carrito::avanzar));
    ejecutarFasePrueba("PRUEBA: REVERSA",  "prueba_reversa",  static_cast<void (Carrito::*)(int)>(&Carrito::reversa));
    ejecutarFasePrueba("PRUEBA: GIRO DER", "prueba_giro_der", static_cast<void (Carrito::*)(int)>(&Carrito::girarDerecha));
    ejecutarFasePrueba("PRUEBA: GIRO IZQ", "prueba_giro_izq", static_cast<void (Carrito::*)(int)>(&Carrito::girarIzquierda));

    carrito.detener();
    modoActual = MODO_SEGURO;

    if (oledAvailable) oled.showMessage("PRUEBA OK", rtc.getTime24());
    logEvent(ACTION_LOG, "prueba_finalizada");
}

// ---------------------------------------------------------------------------
// Pantalla y registros
// ---------------------------------------------------------------------------

void updateDisplay() {
    if (modoActual == MODO_SEGURO) {
        oled.showMessage("MODO SEGURO", rtc.getTime24());
        return;
    }

    oled.printCentered(0, rtc.getTime24());
    oled.printCentered(2, rtc.getDateLatin());
    oled.printCentered(3, rtc.getDayName(true));
}

void logStatus() {
    if (!rtcAvailable) return;

    String line = timestamp();
    line += ",carrito=";
    line += carritoAvailable ? "listo" : "error";
    line += ",modo=";
    line += modeName();
    sd.appendLine(ACTION_LOG, line);
}

void logEvent(const char* path, const String& event) {
    if (!sdAvailable) return;

    String line = rtcAvailable ? timestamp() : "sin_rtc";
    line += ",";
    line += event;
    sd.appendLine(path, line);
}

String timestamp() {
    return rtc.getDateTimeLatin();
}

const char* modeName() {
    switch (modoActual) {
        case MODO_MANUAL: return "manual";
        case MODO_AUTOMATICO: return "automatico";
        case MODO_SEGUIDOR_LINEA: return "seguidor_linea";
        case MODO_SEGURO: return "seguro";
        default: return "desconocido";
    }
}
