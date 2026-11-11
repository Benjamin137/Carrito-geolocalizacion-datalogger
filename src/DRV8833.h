
#ifndef DRV8833_H
#define DRV8833_H

#include <Arduino.h>

class DRV8833 {
public:

    enum Direccion {
        BACKWARD = -1,
        STOP = 0,
        FORWARD = 1
    };

    DRV8833(
        uint8_t ain1,
        uint8_t ain2,
        uint8_t bin1,
        uint8_t bin2,
        bool invertirA = false,
        bool invertirB = false,
        uint32_t frecuenciaPWM = 20000,
        uint8_t resolucionPWM = 8,
        uint8_t canalBase = 0
    ) :
        _ain1(ain1),
        _ain2(ain2),
        _bin1(bin1),
        _bin2(bin2),
        _invertirA(invertirA),
        _invertirB(invertirB),
        _frecuenciaPWM(frecuenciaPWM),
        _resolucionPWM(resolucionPWM),
        _maxPWM((1U << resolucionPWM) - 1),
        _inicializado(false),
        _canalBase(canalBase) {
    }

    bool begin() {
        if (_resolucionPWM < 1 || _resolucionPWM > 16) return false;

        bool ok = true;
        if (_canalBase > 12 || _canalBase + 3 >= 16) return false;

        for (uint8_t indice = 0; indice < 4; ++indice) {
            ok &= ledcSetup(_canalBase + indice, _frecuenciaPWM, _resolucionPWM) != 0;
        }

        ledcAttachPin(_ain1, _canalBase);
        ledcAttachPin(_ain2, _canalBase + 1);

        ledcAttachPin(_bin1, _canalBase + 2);
        ledcAttachPin(_bin2, _canalBase + 3);

        if (!ok) return false;

        _inicializado = true;
        stop();
        return true;
    }

    void motorA(int velocidad) {
        controlarMotor(_ain1, _ain2, velocidad, _invertirA);
    }

    void motorB(int velocidad) {
        controlarMotor(_bin1, _bin2, velocidad, _invertirB);
    }

    void setSpeed(int velocidad) {
        motorA(velocidad);
    }

    void stop() {
        motorA(0);
        motorB(0);
    }

    void setInversionA(bool invertir) {
        _invertirA = invertir;
    }

    void setInversionB(bool invertir) {
        _invertirB = invertir;
    }

private:
    uint8_t _ain1;
    uint8_t _ain2;
    uint8_t _bin1;
    uint8_t _bin2;
    bool _invertirA;
    bool _invertirB;
    uint32_t _frecuenciaPWM;
    uint8_t _resolucionPWM;
    uint16_t _maxPWM;
    bool _inicializado;
    uint8_t _canalBase;

    void controlarMotor(uint8_t pin1, uint8_t pin2, int velocidad, bool invertir) {
        if (!_inicializado) return;

        velocidad = constrain(velocidad, -255, 255);
        if (invertir) velocidad = -velocidad;

        uint16_t pwm = map(abs(velocidad), 0, 255, 0, _maxPWM);

        if (velocidad > 0) {
            escribirPWM(pin1, pwm);
            escribirPWM(pin2, 0);
        } else if (velocidad < 0) {
            escribirPWM(pin1, 0);
            escribirPWM(pin2, pwm);
        } else {
            escribirPWM(pin1, 0);
            escribirPWM(pin2, 0);
        }
    }

    void escribirPWM(uint8_t pin, uint16_t valor) {
        uint8_t canal;

        if (pin == _ain1) canal = _canalBase;
        else if (pin == _ain2) canal = _canalBase + 1;
        else if (pin == _bin1) canal = _canalBase + 2;
        else canal = _canalBase + 3;

        ledcWrite(canal, valor);
    }
};

#endif
