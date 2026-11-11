#ifndef GPS_SENSOR_H
#define GPS_SENSOR_H

#include <Arduino.h>
#include <HardwareSerial.h>
#include <TinyGPSPlus.h>

// Clase para receptores GPS NMEA compatibles con TinyGPSPlus.
class GPSSensor {
public:
  GPSSensor(HardwareSerial& serial, int8_t rxPin, int8_t txPin,
            uint32_t baud = 9600)
      : _serial(serial), _rx(rxPin), _tx(txPin), _baud(baud),
        _lastUpdate(0) {}

  void begin() { _serial.begin(_baud, SERIAL_8N1, _rx, _tx); }

  bool update() {
    bool updated = false;
    while (_serial.available()) {
      if (_gps.encode(_serial.read())) updated = true;
    }
    if (updated) _lastUpdate = millis();
    return updated;
  }

  bool hasLocation() const { return _gps.location.isValid(); }
  bool hasDate() const { return _gps.date.isValid(); }
  bool hasTime() const { return _gps.time.isValid(); }
  double latitude() { return _gps.location.lat(); }
  double longitude() { return _gps.location.lng(); }
  double altitudeMeters() { return _gps.altitude.meters(); }
  double speedKmph() { return _gps.speed.kmph(); }
  uint32_t satellites() { return _gps.satellites.value(); }
  unsigned long getLastUpdateTime() const { return _lastUpdate; }

private:
  HardwareSerial& _serial;
  TinyGPSPlus _gps;
  int8_t _rx, _tx;
  uint32_t _baud;
  unsigned long _lastUpdate;
};

#endif
