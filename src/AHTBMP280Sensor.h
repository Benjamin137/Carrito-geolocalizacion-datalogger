#ifndef AHT_BMP280_SENSOR_H
#define AHT_BMP280_SENSOR_H

#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_AHTX0.h>
#include <Adafruit_BMP280.h>

class AHTBMP280Sensor {
public:
  AHTBMP280Sensor(uint8_t ahtAddress = 0x38, uint8_t bmpAddress = 0x76,
                  int sda = 21, int scl = 22)
      : _ahtAddress(ahtAddress), _bmpAddress(bmpAddress), _sda(sda), _scl(scl) {}

  bool begin() {
    Wire.begin(_sda, _scl);
    Wire.setClock(100000);

    _ahtOk = probe(_ahtAddress) && _aht.begin(&Wire, 0, _ahtAddress);

    _bmpOk = beginBmp(_bmpAddress);
    if (!_bmpOk) {
      uint8_t alt = (_bmpAddress == 0x76) ? 0x77 : 0x76;
      if (beginBmp(alt)) { _bmpOk = true; _bmpAddress = alt; }
    }
    if (_bmpOk) {
      _bmp.setSampling(Adafruit_BMP280::MODE_NORMAL,
                       Adafruit_BMP280::SAMPLING_X2,   // temperatura
                       Adafruit_BMP280::SAMPLING_X16,  // presión
                       Adafruit_BMP280::FILTER_X16,
                       Adafruit_BMP280::STANDBY_MS_500);
    }

    // Algunas versiones del core reinician pines/clock dentro de begin()
    Wire.begin(_sda, _scl);
    Wire.setClock(100000);

    Serial.printf("AHT20 0x%02X: %s, BMP280 0x%02X: %s\n",
                  _ahtAddress, _ahtOk ? "OK" : "NO ENCONTRADO",
                  _bmpAddress, _bmpOk ? "OK" : "NO ENCONTRADO");
    return _ahtOk || _bmpOk;
  }

  // Devuelve true si al menos un sensor entregó datos válidos.
  bool update() {
    bool any = false;

    if (_ahtOk) {
      sensors_event_t h, t;
      if (_aht.getEvent(&h, &t) &&
          !isnan(t.temperature) && !isnan(h.relative_humidity)) {
        _temperature = t.temperature;
        _humidity = constrain(h.relative_humidity, 0.0f, 100.0f);
        any = true;
      }
    }

    if (_bmpOk) {
      float p = _bmp.readPressure();
      if (!isnan(p) && p >= 30000.0f && p <= 120000.0f) {
        _pressure = p / 100.0f;
        // Respaldo de temperatura si el AHT20 no está disponible
        if (!_ahtOk) _temperature = _bmp.readTemperature();
        any = true;
      }
    }

    if (any) _lastUpdateMs = millis();
    return any;
  }

  bool isStarted() const { return _ahtOk || _bmpOk; }
  bool hasAHT() const { return _ahtOk; }
  bool hasBMP() const { return _bmpOk; }
  bool isFresh(uint32_t maxAgeMs = 10000) const {
    return _lastUpdateMs != 0 && (millis() - _lastUpdateMs) <= maxAgeMs;
  }

  float getTemperature() const { return _temperature; }
  float getHumidity() const { return _humidity; }
  float getPressureHpa() const { return _pressure; }
  float getPressurePa() const { return _pressure * 100.0f; }

  // Presión reducida a nivel del mar (hPa), dada tu altitud en metros.
  float getSeaLevelPressureHpa(float altitudeM) const {
    if (isnan(_pressure)) return NAN;
    return _pressure / powf(1.0f - (altitudeM / 44330.0f), 5.255f);
  }

private:
  bool probe(uint8_t address) {
    Wire.beginTransmission(address);
    return Wire.endTransmission() == 0;
  }

  // Acepta BMP280 (0x58) y BME280 (0x60)
  bool beginBmp(uint8_t address) {
    if (!probe(address)) return false;
    return _bmp.begin(address, BMP280_CHIPID) || _bmp.begin(address, 0x60);
  }

  Adafruit_AHTX0 _aht;
  Adafruit_BMP280 _bmp;
  uint8_t _ahtAddress, _bmpAddress;
  int _sda, _scl;
  bool _ahtOk = false, _bmpOk = false;
  uint32_t _lastUpdateMs = 0;
  float _temperature = NAN, _humidity = NAN, _pressure = NAN;
};

#endif