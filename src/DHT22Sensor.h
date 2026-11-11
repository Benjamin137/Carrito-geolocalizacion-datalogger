#ifndef DHT22SENSOR_H
#define DHT22SENSOR_H

#include <Arduino.h>
#include <DHT.h>   // Librería "DHT sensor library" de Adafruit (+ "Adafruit Unified Sensor")

// Clase para el sensor DHT22 (todo en un solo .h).
// HEREDA de la clase DHT de Adafruit, por lo que conservas todas sus funciones
// originales (readTemperature, readHumidity, computeHeatIndex, convertCtoF...)
// y se le agregan lecturas periódicas no bloqueantes con valores en caché.
class DHT22Sensor : public DHT {
public:
  // pin: GPIO donde está conectado el pin DATA del DHT22.
  // readIntervalMs: cada cuántos ms se actualiza la lectura (mínimo 2000, límite del DHT22).
  DHT22Sensor(uint8_t pin, unsigned long readIntervalMs = 2000)
    : DHT(pin, DHT22),
      _interval(readIntervalMs < 2000 ? 2000 : readIntervalMs),
      _lastRead(0), _temperature(NAN), _humidity(NAN),
      _valid(false), _failCount(0) {}

  // Llamar una vez en setup(): inicializa el sensor.
  void begin(uint8_t usec = 55) {
    DHT::begin(usec);
    _lastRead = millis();  // la primera lectura ocurrirá tras el primer intervalo
  }

  // Llamar en cada vuelta del loop().
  // Devuelve true solo cuando se obtuvo una lectura NUEVA y válida.
  bool update() {
    unsigned long now = millis();
    if (now - _lastRead < _interval) return false;
    _lastRead = now;
    return readNow();
  }

  // Fuerza una lectura inmediata (ignora el intervalo). Devuelve true si fue válida.
  bool readNow() {
    // Solo se fuerza UNA lectura al sensor; la humedad se toma de esa misma
    // lectura (el DHT22 no admite dos lecturas con menos de 2 s de diferencia).
    float t = readTemperature(false, true);
    float h = readHumidity();

    if (isnan(t) || isnan(h)) {
      _valid = false;
      _failCount++;
      return false;   // se conservan los últimos valores buenos
    }

    _temperature = t;
    _humidity = h;
    _valid = true;
    return true;
  }

  // ---- Últimos valores leídos (NAN si aún no hay lectura válida) ----
  float getTemperature()  const { return _temperature; }                       // °C
  float getTemperatureF() const { return _temperature * 1.8f + 32.0f; }        // °F
  float getHumidity()     const { return _humidity; }                          // %

  // Sensación térmica en °C.
  float getHeatIndex() {
    if (isnan(_temperature) || isnan(_humidity)) return NAN;
    return computeHeatIndex(_temperature, _humidity, false);
  }

  // Punto de rocío en °C (fórmula de Magnus).
  float getDewPoint() const {
    if (isnan(_temperature) || isnan(_humidity) || _humidity <= 0) return NAN;
    const float a = 17.62f, b = 243.12f;
    float g = log(_humidity / 100.0f) + (a * _temperature) / (b + _temperature);
    return (b * g) / (a - g);
  }

  // true si la última lectura fue válida.
  bool isValid() const { return _valid; }

  // Cantidad de lecturas fallidas desde el arranque (útil para detectar fallas de cableado).
  uint32_t getFailCount() const { return _failCount; }

  // Cambia el intervalo de lectura (mínimo 2000 ms).
  void setInterval(unsigned long ms) { _interval = (ms < 2000) ? 2000 : ms; }

private:
  unsigned long _interval, _lastRead;
  float _temperature, _humidity;
  bool _valid;
  uint32_t _failCount;
};

#endif
