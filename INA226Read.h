#ifndef INA226READ_H
#define INA226READ_H

#include <Arduino.h>
#include <Wire.h>

// INA226Read 类：用于读取 INA226 的电压、电流、功率等数据
class INA226Read {
public:
  /**
   * 构造函数
   * @param address     INA226 的 I2C 地址，常见是 0x40~0x4F，你这里是 0x44
   * @param rshunt      分流电阻值（单位：欧姆），例如 0.1 表示 0.1Ω
   * @param currentLsb  电流分辨率（单位：A/bit），这里先用 0.001 即 1mA/bit
   */
  INA226Read(uint8_t address = 0x44, float rshunt = 0.1f, float currentLsb = 0.001f);

  /**
   * 初始化 INA226 和 I2C
   * @param sda         SDA 引脚号
   * @param scl         SCL 引脚号
   * @param wirePort    使用哪个 Wire 对象，默认就是 Wire
   * @return            true 表示初始化看起来正常，false 表示可能没读到芯片
   */
  bool begin(int sda, int scl, TwoWire &wirePort = Wire);

  /**
   * 设置配置寄存器
   * 一般默认值就可以，不一定需要改
   */
  void setConfig(uint16_t config);

  /**
   * 设置校准寄存器
   * 只有写入校准值后，电流/功率读取才会正确
   */
  void setCalibration();

  // 读取母线电压（单位：V）
  float readBusVoltage();

  // 读取分流电阻两端压降（单位：mV）
  float readShuntVoltage_mV();

  // 读取电流（单位：A）
  float readCurrent();

  // 读取功率（单位：W）
  float readPower();

  // 读取厂家 ID
  uint16_t readManufacturerID();

  // 读取芯片 ID
  uint16_t readDieID();

private:
  // 保存当前使用的 I2C 对象
  TwoWire* _wire;

  // 芯片 I2C 地址
  uint8_t _address;

  // 分流电阻值（欧姆）
  float _rshunt;

  // 电流最小分辨率（A/bit）
  float _currentLsb;

  // 计算出来的校准值，会写入芯片寄存器
  uint16_t _calibration;

  // INA226 的配置寄存器缓存值
  uint16_t _config;

  // 向某个 16 位寄存器写数据
  void writeRegister16(uint8_t reg, uint16_t value);

  // 从某个 16 位寄存器读无符号数据
  uint16_t readRegister16(uint8_t reg);

  // 从某个 16 位寄存器读有符号数据
  int16_t readRegisterS16(uint8_t reg);
};

#endif