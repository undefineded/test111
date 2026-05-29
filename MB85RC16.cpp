#include "MB85RC16.h"

MB85RC16::MB85RC16(uint8_t baseAddress) {
  _baseAddress = baseAddress;
  _wire = &Wire;
}

bool MB85RC16::begin(TwoWire &wirePort) {
  _wire = &wirePort;
  return isConnected();
}

bool MB85RC16::isConnected() {
  _wire->beginTransmission(_baseAddress);
  return (_wire->endTransmission() == 0);
}

uint8_t MB85RC16::calcDeviceAddr(uint16_t memAddress) {
  if (memAddress >= MB85RC16_CAPACITY) memAddress = MB85RC16_CAPACITY - 1;
  uint8_t upperBits = (memAddress >> 8) & 0x07;
  return _baseAddress | upperBits;
}

uint8_t MB85RC16::readByte(uint16_t address) {
  if (address >= MB85RC16_CAPACITY) return 0xFF;

  uint8_t devAddr = calcDeviceAddr(address);
  uint8_t wordAddr = address & 0xFF;

  _wire->beginTransmission(devAddr);
  _wire->write(wordAddr);
  _wire->endTransmission(false);

  _wire->requestFrom((uint8_t)devAddr, (uint8_t)1);
  if (_wire->available()) {
    return _wire->read();
  }
  return 0xFF;
}

void MB85RC16::writeByte(uint16_t address, uint8_t data) {
  if (address >= MB85RC16_CAPACITY) return;

  uint8_t devAddr = calcDeviceAddr(address);
  uint8_t wordAddr = address & 0xFF;

  _wire->beginTransmission(devAddr);
  _wire->write(wordAddr);
  _wire->write(data);
  _wire->endTransmission();
}

uint16_t MB85RC16::readBytes(uint16_t address, uint8_t *buffer, uint16_t length) {
  if (address + length > MB85RC16_CAPACITY) {
    length = MB85RC16_CAPACITY - address;
  }
  if (length == 0) return 0;

  uint16_t bytesRead = 0;
  uint16_t currentAddr = address;
  uint16_t remaining = length;

  while (remaining > 0) {
    uint8_t devAddr = calcDeviceAddr(currentAddr);
    uint8_t wordAddr = currentAddr & 0xFF;
    uint16_t blockRemaining = MB85RC16_PAGE_SIZE - wordAddr;
    uint16_t chunkSize = (remaining < blockRemaining) ? remaining : blockRemaining;

    _wire->beginTransmission(devAddr);
    _wire->write(wordAddr);
    _wire->endTransmission(false);

    _wire->requestFrom((uint8_t)devAddr, (uint8_t)chunkSize);
    for (uint16_t i = 0; i < chunkSize && _wire->available(); i++) {
      buffer[bytesRead++] = _wire->read();
    }

    currentAddr += chunkSize;
    remaining -= chunkSize;
  }

  return bytesRead;
}

uint16_t MB85RC16::writeBytes(uint16_t address, const uint8_t *data, uint16_t length) {
  if (address + length > MB85RC16_CAPACITY) {
    length = MB85RC16_CAPACITY - address;
  }
  if (length == 0) return 0;

  uint16_t bytesWritten = 0;
  uint16_t currentAddr = address;
  uint16_t remaining = length;

  while (remaining > 0) {
    uint8_t devAddr = calcDeviceAddr(currentAddr);
    uint8_t wordAddr = currentAddr & 0xFF;
    uint16_t blockRemaining = MB85RC16_PAGE_SIZE - wordAddr;
    uint16_t chunkSize = (remaining < blockRemaining) ? remaining : blockRemaining;

    _wire->beginTransmission(devAddr);
    _wire->write(wordAddr);
    for (uint16_t i = 0; i < chunkSize; i++) {
      _wire->write(data[bytesWritten + i]);
    }
    _wire->endTransmission();

    bytesWritten += chunkSize;
    currentAddr += chunkSize;
    remaining -= chunkSize;
  }

  return bytesWritten;
}

void MB85RC16::fill(uint16_t startAddress, uint16_t endAddress, uint8_t value) {
  if (startAddress >= MB85RC16_CAPACITY) return;
  if (endAddress >= MB85RC16_CAPACITY) endAddress = MB85RC16_CAPACITY - 1;
  if (startAddress > endAddress) return;

  for (uint16_t addr = startAddress; addr <= endAddress; addr++) {
    writeByte(addr, value);
  }
}

uint16_t MB85RC16::readDeviceID() {
  _wire->beginTransmission((uint8_t)0x7C);
  _wire->write((uint8_t)(_baseAddress << 1));
  _wire->endTransmission(false);

  _wire->requestFrom((uint8_t)0x7C, (uint8_t)4);
  if (_wire->available() >= 4) {
    uint8_t mfgMSB = _wire->read();
    uint8_t mfgLSB = _wire->read();
    uint8_t prodMSB = _wire->read();
    uint8_t prodLSB = _wire->read();
    uint16_t manufacturerId = ((uint16_t)mfgMSB << 4) | (mfgLSB >> 4);
    uint16_t productId = (((uint16_t)mfgLSB & 0x0F) << 8) | prodMSB;
    (void)prodLSB;
    return productId;
  }
  return 0xFFFF;
}