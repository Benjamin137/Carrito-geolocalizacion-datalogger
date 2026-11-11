#ifndef HCSR04_SENSOR_H
#define HCSR04_SENSOR_H

#include <Arduino.h>

// Clase para sensor ultrasónico HC-SR04.
// El pin ECHO debe recibir un nivel de 3.3 V compatible con el ESP32.
class HCSR04Sensor {
public:
  HCSR04Sensor(uint8_t triggerPin, uint8_t echoPin,
               unsigned long timeoutUs = 30000)
      : _trigger(triggerPin), _echo(echoPin), _timeout(timeoutUs),
        _lastDistance(NAN), _lastRead(0) {}

  void begin() {
    pinMode(_trigger, OUTPUT);
    pinMode(_echo, INPUT);
    digitalWrite(_trigger, LOW);
  }

  float readCentimeters() {
    digitalWrite(_trigger, LOW);
    delayMicroseconds(2);
    digitalWrite(_trigger, HIGH);
    delayMicroseconds(10);
    digitalWrite(_trigger, LOW);

    unsigned long duration = pulseIn(_echo, HIGH, _timeout);
    _lastRead = millis();
    if (duration == 0) {
      _lastDistance = NAN;
      return NAN;
    }

    _lastDistance = duration / 58.0f;
    return _lastDistance;
  }

  float readInches() {
    float centimeters = readCentimeters();
    return isnan(centimeters) ? NAN : centimeters / 2.54f;
  }

  float getLastDistanceCm() const { return _lastDistance; }
  unsigned long getLastReadTime() const { return _lastRead; }

private:
  uint8_t _trigger, _echo;
  unsigned long _timeout, _lastRead;
  float _lastDistance;
};

#endif
