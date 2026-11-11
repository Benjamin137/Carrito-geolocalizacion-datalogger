#ifndef LORA_MODULE_H
#define LORA_MODULE_H

#include <Arduino.h>
#include <SPI.h>
#include <LoRa.h>

// Clase para módulos LoRa compatibles con la librería LoRa de Sandeep Mistry.
// Los pines y la frecuencia se pueden adaptar al módulo instalado.
class LoRaModule {
public:
  LoRaModule(long frequency = 915E6, int8_t cs = 5, int8_t reset = 14,
             int8_t irq = 2, int8_t sck = 18, int8_t miso = 19,
             int8_t mosi = 23)
      : _frequency(frequency), _cs(cs), _reset(reset), _irq(irq),
        _sck(sck), _miso(miso), _mosi(mosi), _started(false) {}

  bool begin() {
    _spi.begin(_sck, _miso, _mosi, _cs);
    LoRa.setSPI(_spi);
    LoRa.setPins(_cs, _reset, _irq);
    _started = LoRa.begin(_frequency);
    return _started;
  }

  bool isStarted() const { return _started; }

  bool send(const String& message) {
    if (!_started) return false;
    if (!LoRa.beginPacket()) return false;
    LoRa.print(message);
    return LoRa.endPacket() == 1;
  }

  int parsePacket() {
    if (!_started) return 0;
    return LoRa.parsePacket();
  }

  String readPacket() {
    String message;
    while (LoRa.available()) message += (char)LoRa.read();
    return message;
  }

  long packetRssi() const { return LoRa.packetRssi(); }
  float packetSnr() const { return LoRa.packetSnr(); }

  void setTxPower(int level) { LoRa.setTxPower(level); }
  void setSpreadingFactor(uint8_t factor) { LoRa.setSpreadingFactor(factor); }
  void setSignalBandwidth(long bandwidth) { LoRa.setSignalBandwidth(bandwidth); }
  void setCodingRate4(uint8_t denominator) { LoRa.setCodingRate4(denominator); }
  void setSyncWord(uint8_t word) { LoRa.setSyncWord(word); }

private:
  long _frequency;
  int8_t _cs, _reset, _irq, _sck, _miso, _mosi;
  bool _started;
  SPIClass _spi = SPIClass(VSPI);
};

#endif
