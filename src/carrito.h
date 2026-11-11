#ifndef CARRITO_H
#define CARRITO_H

#include <Arduino.h>
#include "DRV8833.h"

// Clase de alto nivel para un carrito con dos drivers DRV8833.
// Cada driver controla los motores delantero y trasero de un lado.
class Carrito {
public:
  Carrito(DRV8833& ladoIzquierdo, DRV8833& ladoDerecho)
      : _ladoIzquierdo(ladoIzquierdo), _ladoDerecho(ladoDerecho),
        _velocidad(255), _inicializado(false) {}

  bool begin() {
    bool izquierdaOk = _ladoIzquierdo.begin();
    bool derechaOk = _ladoDerecho.begin();
    _inicializado = izquierdaOk && derechaOk;
    
    if (_inicializado) detener();
    Serial.print("Carrito inicializado: ");
    return _inicializado;
  }

  void setVelocidad(int velocidad) {
    _velocidad = constrain(velocidad, 0, 255);
  }

  int getVelocidad() const { return _velocidad; }

  void avanzar() {
    mover(_velocidad, _velocidad);
  }

  void avanzar(int velocidad) {
    setVelocidad(velocidad);
    avanzar();
  }

  void reversa() {
    mover(-_velocidad, -_velocidad);
  }

  void reversa(int velocidad) {
    setVelocidad(velocidad);
    reversa();
  }

  void girarDerecha() {
    mover(_velocidad, -_velocidad);
  }

  void girarDerecha(int velocidad) {
    setVelocidad(velocidad);
    girarDerecha();
  }

  void girarIzquierda() {
    mover(-_velocidad, _velocidad);
  }

  void girarIzquierda(int velocidad) {
    setVelocidad(velocidad);
    girarIzquierda();
  }

  void detener() {
    _ladoIzquierdo.stop();
    _ladoDerecho.stop();
  }

  bool isInitialized() const { return _inicializado; }

private:
  DRV8833& _ladoIzquierdo;
  DRV8833& _ladoDerecho;
  int _velocidad;
  bool _inicializado;

  void mover(int velocidadIzquierda, int velocidadDerecha) {
    if (!_inicializado) return;
    _ladoIzquierdo.motorA(velocidadIzquierda);
    _ladoIzquierdo.motorB(velocidadIzquierda);
    _ladoDerecho.motorA(velocidadDerecha);
    _ladoDerecho.motorB(velocidadDerecha);
  }
};

#endif
