#ifndef BUZZER_H
#define BUZZER_H

#include <Arduino.h>

// Clase para buzzer PASIVO en ESP32 (todo en un solo .h).
// Compatible con Arduino-ESP32 core 2.x y 3.x (PWM por LEDC).
// No bloqueante: llama a update() en cada vuelta del loop().
//
// NOTA: un buzzer ACTIVO solo suena a una frecuencia fija y no admite
// volumen ni melodías; esta clase necesita un buzzer pasivo.
class Buzzer {
public:
  // Cómo sonará el tono en play_buzzer()
  enum Mode {
    CONTINUOUS,  // tono fijo y continuo
    BEEP,        // intermitente (pitidos: suena / silencio)
    SIREN        // sirena: la frecuencia sube y baja
  };

  // pin: GPIO del buzzer.
  // channel: canal LEDC (solo se usa en core 2.x y 3.x con ledcAttachChannel).
  //          Por defecto 4 para no chocar con RGBLed (usa canales 0-2).
  Buzzer(uint8_t pin, uint8_t channel = 4)
    : _pin(pin), _channel(channel), _source(SRC_NONE),
      _duty(0), _sounding(false), _curFreq(0),
      _toneFreq(0), _toneDuration(0), _toneMode(CONTINUOUS),
      _modeInterval(200), _toneStart(0),
      _notes(nullptr), _noteCount(0), _noteIdx(0), _loopsLeft(0), _noteStart(0) {}

  // Llamar una vez en setup().
  void begin() {
#if defined(ESP_ARDUINO_VERSION_MAJOR) && ESP_ARDUINO_VERSION_MAJOR >= 3
    ledcAttachChannel(_pin, 2000, PWM_RES, _channel);
#else
    ledcSetup(_channel, 2000, PWM_RES);
    ledcAttachPin(_pin, _channel);
#endif
    writeDuty(0);
  }

  // ---------------------------------------------------------------
  // play_buzzer: reproduce un tono.
  //   frequencyHz : frecuencia del sonido (ej. 1000 = agudo medio, 400 = grave)
  //   durationMs  : cuánto dura en ms (0 = suena hasta llamar stop_buzzer())
  //   volume      : "potencia" 0-100 (0 = silencio, 100 = máximo)
  //   mode        : CONTINUOUS, BEEP o SIREN
  //   modeInterval: BEEP  -> ms de sonido y ms de silencio
  //                 SIREN -> ms que tarda en subir de frecuencia (luego baja)
  // ---------------------------------------------------------------
  void play_buzzer(uint16_t frequencyHz, unsigned long durationMs,
                   uint8_t volume = 50, Mode mode = CONTINUOUS,
                   unsigned long modeInterval = 200) {
    setVolume(volume);
    _toneFreq = frequencyHz;
    _toneDuration = durationMs;
    _toneMode = mode;
    _modeInterval = (modeInterval == 0) ? 1 : modeInterval;
    _toneStart = millis();
    _source = SRC_TONE;
    silence();
    update();  // arranca de inmediato
  }

  // Detiene cualquier sonido.
  void stop_buzzer() {
    _source = SRC_NONE;
    silence();
  }

  // ---- Sonidos predefinidos ----

  // Alerta: tres pitidos medios.
  void play_alert(uint8_t volume = 60) {
    static const Note notes[] = {
      {1200, 200}, {0, 120}, {1200, 200}, {0, 120}, {1200, 200}
    };
    startMelody(notes, sizeof(notes) / sizeof(notes[0]), 1, volume);
  }

  // Peligro: sirena rápida y aguda alternando dos tonos (~2.4 s).
  void play_danger(uint8_t volume = 100) {
    static const Note notes[] = {
      {2800, 150}, {1800, 150}
    };
    startMelody(notes, sizeof(notes) / sizeof(notes[0]), 8, volume);
  }

  // Canción de inicio del sistema: arpegio ascendente (Do-Mi-Sol-Do).
  void start_system(uint8_t volume = 50) {
    static const Note notes[] = {
      {N_C5, 120}, {N_E5, 120}, {N_G5, 120}, {N_C6, 350}
    };
    startMelody(notes, sizeof(notes) / sizeof(notes[0]), 1, volume);
  }

  // Canción de apagado del sistema: arpegio descendente (Do-Sol-Mi-Do).
  void end_system(uint8_t volume = 50) {
    static const Note notes[] = {
      {N_C6, 150}, {N_G5, 150}, {N_E5, 150}, {N_C5, 450}
    };
    startMelody(notes, sizeof(notes) / sizeof(notes[0]), 1, volume);
  }

  // true mientras haya un sonido en reproducción.
  bool is_playing() const { return _source != SRC_NONE; }

  // Bloquea hasta que termine el sonido actual (útil antes de apagar/dormir).
  void wait_done() {
    while (is_playing()) {
      update();
      delay(1);
    }
  }

  // Llamar en cada vuelta del loop().
  void update() {
    if (_source == SRC_TONE) updateTone();
    else if (_source == SRC_MELODY) updateMelody();
  }

private:
  struct Note {
    uint16_t freq;  // Hz (0 = silencio)
    uint16_t ms;    // duración
  };

  enum Source { SRC_NONE, SRC_TONE, SRC_MELODY };

  // Frecuencias de notas usadas en las canciones
  enum : uint16_t { N_C5 = 523, N_E5 = 659, N_G5 = 784, N_C6 = 1047 };

  static const uint8_t  PWM_RES = 10;           // 0-1023
  static const uint16_t MAX_DUTY = 512;         // 50% = máximo volumen en buzzer pasivo

  uint8_t _pin, _channel;
  Source _source;
  uint16_t _duty;
  bool _sounding;
  uint16_t _curFreq;

  // Estado de play_buzzer
  uint16_t _toneFreq;
  unsigned long _toneDuration;
  Mode _toneMode;
  unsigned long _modeInterval;
  unsigned long _toneStart;

  // Estado de melodías
  const Note* _notes;
  uint8_t _noteCount, _noteIdx, _loopsLeft;
  unsigned long _noteStart;

  void setVolume(uint8_t volume) {
    if (volume > 100) volume = 100;
    _duty = (uint32_t)volume * MAX_DUTY / 100;
  }

  void startMelody(const Note* notes, uint8_t count, uint8_t loops, uint8_t volume) {
    setVolume(volume);
    _notes = notes;
    _noteCount = count;
    _noteIdx = 0;
    _loopsLeft = loops;
    _noteStart = millis();
    _source = SRC_MELODY;
    silence();
    update();
  }

  void updateTone() {
    unsigned long elapsed = millis() - _toneStart;

    if (_toneDuration > 0 && elapsed >= _toneDuration) {
      stop_buzzer();
      return;
    }

    switch (_toneMode) {
      case CONTINUOUS:
        sound(_toneFreq);
        break;

      case BEEP:
        sound(((elapsed / _modeInterval) % 2 == 0) ? _toneFreq : 0);
        break;

      case SIREN: {
        unsigned long phase = elapsed % (2 * _modeInterval);
        unsigned long t = (phase < _modeInterval) ? phase : (2 * _modeInterval - phase);
        uint32_t f = _toneFreq + (uint32_t)_toneFreq * t / _modeInterval;  // base -> 2x base
        sound((uint16_t)((f / 10) * 10));
        break;
      }
    }
  }

  void updateMelody() {
    unsigned long now = millis();

    if (now - _noteStart >= _notes[_noteIdx].ms) {
      _noteIdx++;
      if (_noteIdx >= _noteCount) {
        if (_loopsLeft > 1) {
          _loopsLeft--;
          _noteIdx = 0;
        } else {
          stop_buzzer();
          return;
        }
      }
      _noteStart = now;
    }

    const Note& n = _notes[_noteIdx];
    unsigned long elapsed = now - _noteStart;
    // El 15% final de cada nota queda en silencio para separarlas
    sound((elapsed >= (unsigned long)n.ms * 85UL / 100UL) ? 0 : n.freq);
  }

  // Emite una frecuencia (0 = silencio), evitando reescribir si no cambió.
  void sound(uint16_t freq) {
    if (freq == 0 || _duty == 0) {
      silence();
      return;
    }
    if (!_sounding || freq != _curFreq) {
      setFrequency(freq);
      _curFreq = freq;
      writeDuty(_duty);
      _sounding = true;
    }
  }

  void silence() {
    if (_sounding) {
      writeDuty(0);
      _sounding = false;
    }
  }

  void setFrequency(uint16_t freq) {
#if defined(ESP_ARDUINO_VERSION_MAJOR) && ESP_ARDUINO_VERSION_MAJOR >= 3
    ledcChangeFrequency(_pin, freq, PWM_RES);
#else
    ledcSetup(_channel, freq, PWM_RES);
#endif
  }

  void writeDuty(uint16_t duty) {
#if defined(ESP_ARDUINO_VERSION_MAJOR) && ESP_ARDUINO_VERSION_MAJOR >= 3
    ledcWrite(_pin, duty);
#else
    ledcWrite(_channel, duty);
#endif
  }
};

#endif
