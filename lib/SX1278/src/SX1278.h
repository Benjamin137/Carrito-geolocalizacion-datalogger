#ifndef SX1278_H
#define SX1278_H

#include <Arduino.h>
#include <SPI.h>

class SX1278 {
public:
  static const int16_t RECEIVE_NONE = 0;
  static const int16_t RECEIVE_ERROR = -1;
  static const int16_t RECEIVE_BUFFER_TOO_SMALL = -2;

  // Pines por defecto del bus VSPI del ESP32 y del modulo LoRa tipico.
  SX1278(uint8_t chipSelect = 5, uint8_t reset = 14, int8_t dio0 = 2,
         int8_t sck = 18, int8_t miso = 19, int8_t mosi = 23,
         uint32_t spiFrequency = 8000000);

  bool begin(long frequency = 433000000L);
  void end();

  bool send(const uint8_t* data, size_t length, uint32_t timeoutMs = 5000);
  bool send(const char* message, uint32_t timeoutMs = 5000);
  int16_t receive(uint8_t* buffer, size_t bufferSize);
  bool available();

  bool setFrequency(long frequency);
  bool setTxPower(int8_t power);
  bool setSpreadingFactor(uint8_t spreadingFactor);
  bool setSignalBandwidth(long bandwidth);
  bool setCodingRate4(uint8_t denominator);
  void enableCrc(bool enabled);
  void sleep();
  void idle();

  int16_t packetRssi() const;
  float packetSnr() const;
  bool isInitialized() const;

private:
  static const uint8_t REG_FIFO = 0x00;
  static const uint8_t REG_OP_MODE = 0x01;
  static const uint8_t REG_FRF_MSB = 0x06;
  static const uint8_t REG_FRF_MID = 0x07;
  static const uint8_t REG_FRF_LSB = 0x08;
  static const uint8_t REG_PA_CONFIG = 0x09;
  static const uint8_t REG_LNA = 0x0C;
  static const uint8_t REG_FIFO_ADDR_PTR = 0x0D;
  static const uint8_t REG_FIFO_TX_BASE_ADDR = 0x0E;
  static const uint8_t REG_FIFO_RX_BASE_ADDR = 0x0F;
  static const uint8_t REG_FIFO_RX_CURRENT_ADDR = 0x10;
  static const uint8_t REG_IRQ_FLAGS = 0x12;
  static const uint8_t REG_RX_NB_BYTES = 0x13;
  static const uint8_t REG_PKT_SNR_VALUE = 0x19;
  static const uint8_t REG_PKT_RSSI_VALUE = 0x1A;
  static const uint8_t REG_MODEM_CONFIG_1 = 0x1D;
  static const uint8_t REG_MODEM_CONFIG_2 = 0x1E;
  static const uint8_t REG_PREAMBLE_MSB = 0x20;
  static const uint8_t REG_PREAMBLE_LSB = 0x21;
  static const uint8_t REG_PAYLOAD_LENGTH = 0x22;
  static const uint8_t REG_MODEM_CONFIG_3 = 0x26;
  static const uint8_t REG_DIO_MAPPING_1 = 0x40;
  static const uint8_t REG_VERSION = 0x42;
  static const uint8_t REG_PA_DAC = 0x4D;

  static const uint8_t MODE_LONG_RANGE = 0x80;
  static const uint8_t MODE_SLEEP = 0x00;
  static const uint8_t MODE_STDBY = 0x01;
  static const uint8_t MODE_TX = 0x03;
  static const uint8_t MODE_RX_CONTINUOUS = 0x05;
  static const uint8_t IRQ_RX_DONE = 0x40;
  static const uint8_t IRQ_PAYLOAD_CRC_ERROR = 0x20;
  static const uint8_t IRQ_TX_DONE = 0x08;

  uint8_t _cs;
  uint8_t _reset;
  int8_t _dio0;
  SPIClass _spi;
  SPISettings _spiSettings;
  bool _initialized;
  int16_t _lastRssi;
  float _lastSnr;

  uint8_t readRegister(uint8_t address);
  void writeRegister(uint8_t address, uint8_t value);
  void setMode(uint8_t mode);
  bool waitForIrq(uint8_t flag, uint32_t timeoutMs);
  void reset();
};

#endif
