#ifndef RGBLED_H
#define RGBLED_H

#include <Arduino.h>
#include <string.h>

// Clase para LED RGB en ESP32 (todo en un solo .h)
// Compatible con Arduino-ESP32 core 2.x y 3.x (usa PWM por LEDC).
class RGBLed {
public:
  // pinR, pinG, pinB: cualquier GPIO de salida del ESP32.
  // commonAnode = true si tu LED es de ánodo común (lógica invertida).
  RGBLed(uint8_t pinR, uint8_t pinG, uint8_t pinB, bool commonAnode = false)
    : _pinR(pinR), _pinG(pinG), _pinB(pinB), _commonAnode(commonAnode),
      _r(255), _g(255), _b(255),
      _isOn(false), _blinking(false), _interval(0), _lastToggle(0) {}

  // Llamar una vez en setup().
  void begin() {
#if defined(ESP_ARDUINO_VERSION_MAJOR) && ESP_ARDUINO_VERSION_MAJOR >= 3
    ledcAttach(_pinR, PWM_FREQ, PWM_RES);
    ledcAttach(_pinG, PWM_FREQ, PWM_RES);
    ledcAttach(_pinB, PWM_FREQ, PWM_RES);
#else
    _chR = nextChannel(); _chG = nextChannel(); _chB = nextChannel();
    ledcSetup(_chR, PWM_FREQ, PWM_RES); ledcAttachPin(_pinR, _chR);
    ledcSetup(_chG, PWM_FREQ, PWM_RES); ledcAttachPin(_pinG, _chG);
    ledcSetup(_chB, PWM_FREQ, PWM_RES); ledcAttachPin(_pinB, _chB);
#endif
    apply();  // empieza apagado
  }

  // Establece el color por nombre en inglés ("red", "green", "blue"...).
  // No distingue mayúsculas. Devuelve false si el color no existe en la lista.
  bool setColor(const char* name) {
    uint8_t count;
    const NamedColor* list = colors(count);
    for (uint8_t i = 0; i < count; i++) {
      if (strcasecmp(name, list[i].name) == 0) {
        setRGB(list[i].r, list[i].g, list[i].b);
        return true;
      }
    }
    return false;
  }

  bool setColor(const String& name) { return setColor(name.c_str()); }

  // Color personalizado (0-255 por canal).
  void setRGB(uint8_t r, uint8_t g, uint8_t b) {
    _r = r; _g = g; _b = b;
    if (!_blinking) _isOn = true;
    apply();
  }

  // Inicia el parpadeo. intervalMs = ms encendido y ms apagado.
  // Ej: blink(1000) -> cada 1 s, blink(5000) -> cada 5 s.
  void blink(unsigned long intervalMs) {
    _interval = intervalMs;
    _blinking = true;
    _isOn = true;
    _lastToggle = millis();
    apply();
  }

  // Detiene el parpadeo y deja el LED encendido con el color actual.
  void stopBlink() {
    _blinking = false;
    _isOn = true;
    apply();
  }

  // Apaga el LED (y detiene el parpadeo).
  void off() {
    _blinking = false;
    _isOn = false;
    apply();
  }

  // Llamar en cada vuelta del loop() para que el parpadeo funcione.
  void update() {
    if (!_blinking) return;
    unsigned long now = millis();
    if (now - _lastToggle >= _interval) {
      _lastToggle = now;
      _isOn = !_isOn;
      apply();
    }
  }

private:
  struct NamedColor {
    const char* name;
    uint8_t r, g, b;
  };

  // ---- Lista de colores definidos (agrega más aquí) ----
  static const NamedColor* colors(uint8_t& count) {
    static const NamedColor list[] = {
      {"red",     255,   0,   0},
      {"green",     0, 255,   0},
      {"blue",      0,   0, 255},
      {"yellow",  255, 255,   0},
      {"cyan",      0, 255, 255},
      {"magenta", 255,   0, 255},
      {"white",   255, 255, 255},
      {"orange",  255, 100,   0},
      {"purple",  128,   0, 128},
      {"pink",    255,  80, 120}
    };
    count = sizeof(list) / sizeof(list[0]);
    return list;
  }

  static const uint32_t PWM_FREQ = 5000;
  static const uint8_t  PWM_RES  = 8;  // 0-255

  uint8_t _pinR, _pinG, _pinB;
  bool _commonAnode;
  uint8_t _r, _g, _b;
  bool _isOn, _blinking;
  unsigned long _interval, _lastToggle;

#if !(defined(ESP_ARDUINO_VERSION_MAJOR) && ESP_ARDUINO_VERSION_MAJOR >= 3)
  uint8_t _chR = 0, _chG = 0, _chB = 0;
  // Asigna canales LEDC distintos a cada color (y a cada objeto RGBLed).
  static uint8_t nextChannel() {
    static uint8_t ch = 0;
    return ch++;
  }
#endif

  void apply() {
    if (_isOn) {
      writePwm(_pinR, _r, 'r');
      writePwm(_pinG, _g, 'g');
      writePwm(_pinB, _b, 'b');
    } else {
      writePwm(_pinR, 0, 'r');
      writePwm(_pinG, 0, 'g');
      writePwm(_pinB, 0, 'b');
    }
  }

  void writePwm(uint8_t pin, uint8_t value, char color) {
    uint8_t duty = _commonAnode ? 255 - value : value;
#if defined(ESP_ARDUINO_VERSION_MAJOR) && ESP_ARDUINO_VERSION_MAJOR >= 3
    (void)color;
    ledcWrite(pin, duty);
#else
    uint8_t ch = (color == 'r') ? _chR : (color == 'g') ? _chG : _chB;
    (void)pin;
    ledcWrite(ch, duty);
#endif
  }
};

#endif
