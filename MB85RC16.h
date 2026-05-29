#ifndef MB85RC16_H
#define MB85RC16_H

#include <Arduino.h>
#include <Wire.h>

#define MB85RC16_BASE_ADDR  0x50
#define MB85RC16_CAPACITY   2048
#define MB85RC16_PAGE_SIZE  256

class MB85RC16 {
public:
  MB85RC16(uint8_t baseAddress = MB85RC16_BASE_ADDR);

  bool begin(TwoWire &wirePort = Wire);

  bool isConnected();

  uint8_t readByte(uint16_t address);
  void writeByte(uint16_t address, uint8_t data);

  uint16_t readBytes(uint16_t address, uint8_t *buffer, uint16_t length);
  uint16_t writeBytes(uint16_t address, const uint8_t *data, uint16_t length);

  void fill(uint16_t startAddress, uint16_t endAddress, uint8_t value);

  uint16_t readDeviceID();

private:
  TwoWire *_wire;
  uint8_t _baseAddress;

  uint8_t calcDeviceAddr(uint16_t memAddress);
};

#endif // MB85RC16_H