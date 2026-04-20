#include "INA226Read.h"

// -------------------------------
// INA226 寄存器地址定义
// -------------------------------
// 这些数字来自 INA226 芯片手册
#define REG_CONFIG        0x00   // 配置寄存器
#define REG_SHUNT_VOLTAGE 0x01   // 分流电压寄存器
#define REG_BUS_VOLTAGE   0x02   // 母线电压寄存器
#define REG_POWER         0x03   // 功率寄存器
#define REG_CURRENT       0x04   // 电流寄存器
#define REG_CALIBRATION   0x05   // 校准寄存器
#define REG_MANUFACTURER  0xFE   // 厂家 ID
#define REG_DIE_ID        0xFF   // 芯片 ID

// -------------------------------
// 构造函数
// 当你写 INA226Read ina226(0x44, 0.1f, 0.001f);
// 就会执行这里
// -------------------------------
INA226Read::INA226Read(uint8_t address, float rshunt, float currentLsb) {
  _address = address;         // 保存设备地址
  _rshunt = rshunt;           // 保存分流电阻值
  _currentLsb = currentLsb;   // 保存电流分辨率
  _calibration = 0;           // 校准值先清零

  // 默认配置值：
  // AVG=16 次平均
  // 连续测量母线电压 + 分流电压
  _config = 0x4527;

  // 默认使用全局 Wire 对象
  _wire = &Wire;
}

// -------------------------------
// 初始化函数
// 作用：
// 1. 启动 I2C
// 2. 写配置寄存器
// 3. 写校准寄存器
// 4. 试着读一下芯片 ID，看看是否通信正常
// -------------------------------
bool INA226Read::begin(int sda, int scl, TwoWire &wirePort) {
  _wire = &wirePort;

  // 初始化 I2C，总线引脚由你指定
  _wire->begin(sda, scl);

  // 如果需要，也可以设置时钟，例如 100kHz
  // _wire->setClock(100000);

  // 写入配置寄存器
  setConfig(_config);

  // 写入校准值
  setCalibration();

  // 读芯片 ID 作为简单连通性判断
  uint16_t id = readDieID();

  // 如果读到 0x0000 或 0xFFFF，通常说明通信有问题
  return (id != 0x0000 && id != 0xFFFF);
}

// -------------------------------
// 写配置寄存器
// -------------------------------
void INA226Read::setConfig(uint16_t config) {
  _config = config;
  writeRegister16(REG_CONFIG, _config);
}

// -------------------------------
// 写校准寄存器
//
// INA226 的电流/功率不是“自动就准”的，
// 必须先根据分流电阻和 currentLsb 算出校准值，再写进去。
//
// 公式：
// Calibration = 0.00512 / (Current_LSB × Rshunt)
// -------------------------------
void INA226Read::setCalibration() {
  _calibration = (uint16_t)(0.00512f / (_currentLsb * _rshunt));
  writeRegister16(REG_CALIBRATION, _calibration);
}

// -------------------------------
// 向寄存器写入 16 位数据
//
// INA226 寄存器是 16 位的，所以要发两个字节：
// 高字节在前，低字节在后
// -------------------------------
void INA226Read::writeRegister16(uint8_t reg, uint16_t value) {
  _wire->beginTransmission(_address);                // 开始向设备发数据
  _wire->write(reg);                                 // 先发寄存器地址
  _wire->write((uint8_t)((value >> 8) & 0xFF));      // 再发高 8 位
  _wire->write((uint8_t)(value & 0xFF));             // 最后发低 8 位
  _wire->endTransmission();                          // 结束发送
}

// -------------------------------
// 从寄存器读取 16 位“无符号”数据
//
// 步骤：
// 1. 先告诉芯片：我要读哪个寄存器
// 2. 再向芯片请求 2 个字节
// 3. 把两个字节拼成一个 16 位整数
// -------------------------------
uint16_t INA226Read::readRegister16(uint8_t reg) {
  _wire->beginTransmission(_address);
  _wire->write(reg);                // 指定要读的寄存器
  _wire->endTransmission(false);    // false 表示不释放总线，继续读

  // 请求读取 2 个字节
  _wire->requestFrom((uint8_t)_address, (uint8_t)2);

  // 如果可读字节数不足 2，说明通信失败
  if (_wire->available() < 2) {
    return 0;
  }

  // 先读高字节，再读低字节
  uint16_t value = ((uint16_t)_wire->read() << 8) | _wire->read();
  return value;
}

// -------------------------------
// 从寄存器读取 16 位“有符号”数据
//
// 为什么有些寄存器要读成有符号？
// 因为分流电压、电流可能是负值，表示方向反了
// -------------------------------
int16_t INA226Read::readRegisterS16(uint8_t reg) {
  return (int16_t)readRegister16(reg);
}

// -------------------------------
// 读取母线电压（单位：V）
//
// INA226 的 Bus Voltage 寄存器分辨率：1.25mV/bit
// 所以：实际电压 = 原始值 × 0.00125
// -------------------------------
float INA226Read::readBusVoltage() {
  uint16_t raw = readRegister16(REG_BUS_VOLTAGE);
  return raw * 0.00125f;
}

// -------------------------------
// 读取分流电压（单位：mV）
//
// INA226 的 Shunt Voltage 分辨率：2.5uV/bit
// 换成 mV 就是 0.0025mV/bit
//
// 这个值表示：分流电阻两端的压降
// 公式：Vshunt = I × Rshunt
// -------------------------------
float INA226Read::readShuntVoltage_mV() {
  int16_t raw = readRegisterS16(REG_SHUNT_VOLTAGE);
  return raw * 0.0025f;
}

// -------------------------------
// 读取电流（单位：A）
//
// Current 寄存器中的值，需要乘以 currentLsb
// 实际电流 = 原始值 × currentLsb
// -------------------------------
float INA226Read::readCurrent() {
  int16_t raw = readRegisterS16(REG_CURRENT);
  return raw * _currentLsb;
}

// -------------------------------
// 读取功率（单位：W）
//
// INA226 功率寄存器的 LSB = 25 × currentLsb
// 所以：实际功率 = 原始值 × (25 × currentLsb)
// -------------------------------
float INA226Read::readPower() {
  uint16_t raw = readRegister16(REG_POWER);
  float powerLsb = 25.0f * _currentLsb;
  return raw * powerLsb;
}

// -------------------------------
// 读取厂家 ID
// 一般 TI 芯片会有固定值
// -------------------------------
uint16_t INA226Read::readManufacturerID() {
  return readRegister16(REG_MANUFACTURER);
}

// -------------------------------
// 读取芯片 ID
// 用于简单确认芯片是否正常响应
// -------------------------------
uint16_t INA226Read::readDieID() {
  return readRegister16(REG_DIE_ID);
}