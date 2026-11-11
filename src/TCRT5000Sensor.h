#ifndef TCRT5000_SENSOR_H
#define TCRT5000_SENSOR_H

#include <Arduino.h>

// Clase para uno o varios módulos digitales TCRT5000.
// activeLow=true significa que LOW representa línea detectada.
class TCRT5000Sensor {
public:
  TCRT5000Sensor(const uint8_t* pins, uint8_t count, bool activeLow = true)
      : _pins(pins), _count(count), _activeLow(activeLow) {}

  bool begin() {
    if (_pins == nullptr || _count == 0) return false;
    for (uint8_t i = 0; i < _count; ++i) pinMode(_pins[i], INPUT);
    return true;
  }

  bool isLineDetected(uint8_t index) const {
    if (index >= _count) return false;
    bool state = digitalRead(_pins[index]) == HIGH;
    return _activeLow ? !state : state;
  }

  uint32_t readMask() const {
    uint32_t mask = 0;
    uint8_t limit = _count > 32 ? 32 : _count;
    for (uint8_t i = 0; i < limit; ++i) {
      if (isLineDetected(i)) mask |= (1UL << i);
    }
    return mask;
  }

  uint8_t count() const { return _count; }

private:
  const uint8_t* _pins;
  uint8_t _count;
  bool _activeLow;
};

#endif
