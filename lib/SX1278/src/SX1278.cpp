#include "SX1278.h"

SX1278::SX1278(uint8_t chipSelect, uint8_t resetPin, int8_t dio0,
               int8_t sck, int8_t miso, int8_t mosi, uint32_t spiFrequency)
    : _cs(chipSelect), _reset(resetPin), _dio0(dio0), _spi(VSPI),
      _spiSettings(spiFrequency, MSBFIRST, SPI_MODE0), _initialized(false),
      _lastRssi(0), _lastSnr(0.0f) {
  _spi.begin(sck, miso, mosi, chipSelect);
}

bool SX1278::begin(long frequency) {
  pinMode(_cs, OUTPUT);
  digitalWrite(_cs, HIGH);
  pinMode(_reset, OUTPUT);
  if (_dio0 >= 0) {
    pinMode(_dio0, INPUT);
  }

  reset();
  if (readRegister(REG_VERSION) != 0x12) {
    _initialized = false;
    return false;
  }

  setMode(MODE_SLEEP);
  writeRegister(REG_OP_MODE, MODE_LONG_RANGE | MODE_SLEEP);
  writeRegister(REG_FIFO_TX_BASE_ADDR, 0x00);
  writeRegister(REG_FIFO_RX_BASE_ADDR, 0x00);
  writeRegister(REG_LNA, readRegister(REG_LNA) | 0x03);
  writeRegister(REG_MODEM_CONFIG_1, 0x72);  // BW 125 kHz, CR 4/5.
  writeRegister(REG_MODEM_CONFIG_2, 0x74);  // SF7, CRC disabled.
  writeRegister(REG_MODEM_CONFIG_3, 0x04);  // AGC automatic.
  writeRegister(REG_PREAMBLE_MSB, 0x00);
  writeRegister(REG_PREAMBLE_LSB, 0x08);
  writeRegister(REG_DIO_MAPPING_1, 0x00);
  writeRegister(REG_IRQ_FLAGS, 0xFF);

  _initialized = setFrequency(frequency);
  if (_initialized) {
    setMode(MODE_RX_CONTINUOUS);
  }
  return _initialized;
}

void SX1278::end() {
  if (_initialized) {
    sleep();
  }
  _spi.end();
  _initialized = false;
}

bool SX1278::send(const uint8_t* data, size_t length, uint32_t timeoutMs) {
  if (!_initialized || data == nullptr || length == 0 || length > 255) {
    return false;
  }

  setMode(MODE_STDBY);
  writeRegister(REG_FIFO_ADDR_PTR, readRegister(REG_FIFO_TX_BASE_ADDR));
  for (size_t i = 0; i < length; ++i) {
    writeRegister(REG_FIFO, data[i]);
  }
  writeRegister(REG_PAYLOAD_LENGTH, static_cast<uint8_t>(length));
  writeRegister(REG_IRQ_FLAGS, 0xFF);
  setMode(MODE_TX);

  const bool sent = waitForIrq(IRQ_TX_DONE, timeoutMs);
  setMode(MODE_RX_CONTINUOUS);
  return sent;
}

bool SX1278::send(const char* message, uint32_t timeoutMs) {
  if (message == nullptr) {
    return false;
  }
  return send(reinterpret_cast<const uint8_t*>(message), strlen(message), timeoutMs);
}

int16_t SX1278::receive(uint8_t* buffer, size_t bufferSize) {
  if (!_initialized || buffer == nullptr) {
    return RECEIVE_ERROR;
  }
  if (!available()) {
    return RECEIVE_NONE;
  }

  const uint8_t length = readRegister(REG_RX_NB_BYTES);
  if (length > bufferSize) {
    writeRegister(REG_IRQ_FLAGS, IRQ_RX_DONE | IRQ_PAYLOAD_CRC_ERROR);
    return RECEIVE_BUFFER_TOO_SMALL;
  }

  writeRegister(REG_FIFO_ADDR_PTR, readRegister(REG_FIFO_RX_CURRENT_ADDR));
  for (uint8_t i = 0; i < length; ++i) {
    buffer[i] = readRegister(REG_FIFO);
  }
  _lastSnr = static_cast<int8_t>(readRegister(REG_PKT_SNR_VALUE)) / 4.0f;
  _lastRssi = static_cast<int16_t>(readRegister(REG_PKT_RSSI_VALUE)) - 164;
  writeRegister(REG_IRQ_FLAGS, IRQ_RX_DONE);
  return length;
}

bool SX1278::available() {
  if (!_initialized) {
    return false;
  }
  return (readRegister(REG_IRQ_FLAGS) & IRQ_RX_DONE) != 0 &&
         (readRegister(REG_IRQ_FLAGS) & IRQ_PAYLOAD_CRC_ERROR) == 0;
}

bool SX1278::setFrequency(long frequency) {
  if (!_initialized && readRegister(REG_VERSION) != 0x12) {
    return false;
  }
  if (frequency < 137000000L || frequency > 1020000000L) {
    return false;
  }
  const uint64_t frf = (static_cast<uint64_t>(frequency) << 19) / 32000000ULL;
  writeRegister(REG_FRF_MSB, static_cast<uint8_t>(frf >> 16));
  writeRegister(REG_FRF_MID, static_cast<uint8_t>(frf >> 8));
  writeRegister(REG_FRF_LSB, static_cast<uint8_t>(frf));
  return true;
}

bool SX1278::setTxPower(int8_t power) {
  if (!_initialized || power < 2 || power > 20) {
    return false;
  }
  if (power > 17) {
    writeRegister(REG_PA_DAC, 0x87);
    power -= 3;
  } else {
    writeRegister(REG_PA_DAC, 0x84);
  }
  writeRegister(REG_PA_CONFIG, 0x80 | static_cast<uint8_t>(power - 2));
  return true;
}

bool SX1278::setSpreadingFactor(uint8_t spreadingFactor) {
  if (!_initialized || spreadingFactor < 6 || spreadingFactor > 12) {
    return false;
  }
  uint8_t config = readRegister(REG_MODEM_CONFIG_2) & 0x0F;
  writeRegister(REG_MODEM_CONFIG_2, config | ((spreadingFactor << 4) & 0xF0));
  writeRegister(REG_MODEM_CONFIG_3, readRegister(REG_MODEM_CONFIG_3) |
                                       (spreadingFactor >= 11 ? 0x08 : 0x00));
  return true;
}

bool SX1278::setSignalBandwidth(long bandwidth) {
  static const long bandwidths[] = {
      7800, 10400, 15600, 20800, 31250, 41700, 62500, 125000, 250000, 500000};
  uint8_t index = 0;
  while (index < 10 && bandwidths[index] != bandwidth) {
    ++index;
  }
  if (!_initialized || index == 10) {
    return false;
  }
  writeRegister(REG_MODEM_CONFIG_1,
                (readRegister(REG_MODEM_CONFIG_1) & 0x0F) | (index << 4));
  return true;
}

bool SX1278::setCodingRate4(uint8_t denominator) {
  if (!_initialized || denominator < 5 || denominator > 8) {
    return false;
  }
  writeRegister(REG_MODEM_CONFIG_1,
                (readRegister(REG_MODEM_CONFIG_1) & 0xF1) |
                    ((denominator - 4) << 1));
  return true;
}

void SX1278::enableCrc(bool enabled) {
  if (_initialized) {
    uint8_t config = readRegister(REG_MODEM_CONFIG_2);
    writeRegister(REG_MODEM_CONFIG_2, enabled ? config | 0x04 : config & 0xFB);
  }
}

void SX1278::sleep() {
  if (_initialized) {
    setMode(MODE_SLEEP);
  }
}

void SX1278::idle() {
  if (_initialized) {
    setMode(MODE_STDBY);
  }
}

int16_t SX1278::packetRssi() const {
  return _lastRssi;
}

float SX1278::packetSnr() const {
  return _lastSnr;
}

bool SX1278::isInitialized() const {
  return _initialized;
}

uint8_t SX1278::readRegister(uint8_t address) {
  _spi.beginTransaction(_spiSettings);
  digitalWrite(_cs, LOW);
  _spi.transfer(address & 0x7F);
  const uint8_t value = _spi.transfer(0x00);
  digitalWrite(_cs, HIGH);
  _spi.endTransaction();
  return value;
}

void SX1278::writeRegister(uint8_t address, uint8_t value) {
  _spi.beginTransaction(_spiSettings);
  digitalWrite(_cs, LOW);
  _spi.transfer(address | 0x80);
  _spi.transfer(value);
  digitalWrite(_cs, HIGH);
  _spi.endTransaction();
}

void SX1278::setMode(uint8_t mode) {
  writeRegister(REG_OP_MODE, MODE_LONG_RANGE | mode);
}

bool SX1278::waitForIrq(uint8_t flag, uint32_t timeoutMs) {
  const unsigned long start = millis();
  while ((millis() - start) < timeoutMs) {
    if ((readRegister(REG_IRQ_FLAGS) & flag) != 0) {
      writeRegister(REG_IRQ_FLAGS, flag);
      return true;
    }
    delay(1);
  }
  return false;
}

void SX1278::reset() {
  digitalWrite(_reset, LOW);
  delay(1);
  digitalWrite(_reset, HIGH);
  delay(5);
}
