#include <Arduino.h>
#include <Wire.h>
#include <U8g2lib.h>
#include "DisplayUI.h"
#include "WebConfig.h"
#include "BleConfig.h"
#include "INA226Read.h"
#include "MB85RC16.h"
#include "soc/usb_serial_jtag_reg.h"

// ==========================================
// 全局变量与对象实例化
// ==========================================

// ===== 铁电存储 =====
MB85RC16 fram;

// 铁电存储地址映射
#define FRAM_ADDR_MODE       0x0000  // WorkMode (1 byte)
#define FRAM_ADDR_PROGRESS   0x0001  // progress (2 bytes, int16_t)
#define FRAM_ADDR_BLE_EN     0x0003  // bleEnabled (1 byte)
#define FRAM_ADDR_FLIP_EN    0x0005  // flipEnabled (1 byte)
#define FRAM_ADDR_SLEEP_EN   0x0006  // sleepEnabled (1 byte)
#define FRAM_ADDR_BRIGHTNESS 0x0008  // brightness (2 bytes, int16_t)
#define FRAM_ADDR_MAGIC      0x000A  // 初始化标记 (1 byte, 0xA5)
#define FRAM_MAGIC_VALUE     0xA5

// ===== 运行模式与全局状态 =====
WorkMode currentWorkMode = MODE_ESC;
portMUX_TYPE mux = portMUX_INITIALIZER_UNLOCKED;

// ===== 引脚定义 =====
#define SDA_PIN 4
#define SCL_PIN 5
#define PWM_PIN 6
#define ENC_A 3
#define ENC_B 18
#define BTN_SET 19

// ===== OLED 与 UI =====
U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, U8X8_PIN_NONE);
DisplayUI ui;

// ===== INA226 传感器 =====
INA226Read ina226(0x40, 0.001f, 0.0025f); // 0.001欧姆采样电阻，满量程 81.92A，电流 LSB 设为 2.5mA
bool ina226Ready = false;

// ===== 编码器与按键状态 =====
volatile int encoderDelta = 0;
volatile uint8_t lastEncoderState = 0;

// ===== 休眠控制 =====
unsigned long lastActivityTime = 0;
bool isScreenOff = false;
bool autoSleepEnabled = true;
const unsigned long SLEEP_TIMEOUT_MS = 30000; // 30秒无操作息屏


// ==========================================
// 辅助函数
// ==========================================

void wakeUpScreen() {
  if (isScreenOff) {
    isScreenOff = false;
    u8g2.setPowerSave(0);
    Serial.println("Screen Woke Up");
  }
  lastActivityTime = millis();
}

void applyPwmConfig(int progress, bool forceReattach = false) {
  static WorkMode lastMode = (WorkMode)-1; // 记录上一次的模式
  bool modeChanged = (currentWorkMode != lastMode);
  
  if (currentWorkMode == MODE_ESC) {
    if (modeChanged || forceReattach) {
      ledcDetach(PWM_PIN); // 先解绑引脚，防止跨度过大的频率重置导致底层驱动死锁
      ledcAttach(PWM_PIN, 50, 12); // 无刷电调：50Hz, 12位分辨率
      lastMode = currentWorkMode;
    }
    // 根据示波器实测波形修正：低油门940μs，高油门2173μs
    // 12位分辨率，50Hz周期20ms，每tick=4.88μs
    // 940μs/4.88≈193, 2173μs/4.88≈445
    // ESC模式下速度为0也必须输出最低油门(193)，否则电调无法完成自检
    int duty = map(progress, 0, 100, 193, 445);
    if (duty < 193) duty = 193; // 保底最低油门
    ledcWrite(PWM_PIN, duty);
  } else if (currentWorkMode == MODE_FAN) {
    if (modeChanged || forceReattach) {
      ledcDetach(PWM_PIN);
      ledcAttach(PWM_PIN, 25000, 10); // 服务器风扇：25kHz, 10位分辨率
      lastMode = currentWorkMode;
    }
    int duty = map(progress, 0, 100, 0, 1023);
    ledcWrite(PWM_PIN, duty);
  }
}


// ==========================================
// 中断服务函数 (ISR)
// ==========================================

// 硬件已经通过 10K+100nF RC电路做了一阶低通滤波(1ms时间常数)
// 故无需软件消抖，直接使用快速状态机解析旋转方向
void IRAM_ATTR isrEncoder() {
  uint8_t a = digitalRead(ENC_A);
  uint8_t b = digitalRead(ENC_B);
  uint8_t currentState = (a << 1) | b;
  
  portENTER_CRITICAL_ISR(&mux);
  if (lastEncoderState != currentState) {
    // 合法顺时针状态转移: 00 -> 01 -> 11 -> 10 -> 00
    if ((lastEncoderState == 0b00 && currentState == 0b01) ||
        (lastEncoderState == 0b01 && currentState == 0b11) ||
        (lastEncoderState == 0b11 && currentState == 0b10) ||
        (lastEncoderState == 0b10 && currentState == 0b00)) {
      encoderDelta++;
    } 
    // 合法逆时针状态转移: 00 -> 10 -> 11 -> 01 -> 00
    else if ((lastEncoderState == 0b00 && currentState == 0b10) ||
             (lastEncoderState == 0b10 && currentState == 0b11) ||
             (lastEncoderState == 0b11 && currentState == 0b01) ||
             (lastEncoderState == 0b01 && currentState == 0b00)) {
      encoderDelta--;
    }
    lastEncoderState = currentState;
  }
  portEXIT_CRITICAL_ISR(&mux);
}


// ==========================================
// FreeRTOS 任务定义
// ==========================================

// ===== UI任务 (5Hz) =====
void taskDisplay(void *pvParameters) {
  TickType_t xLastWakeTime = xTaskGetTickCount();
  const TickType_t xFrequency = pdMS_TO_TICKS(200); 
  while (1) {
    if (!isScreenOff && autoSleepEnabled && (millis() - lastActivityTime > SLEEP_TIMEOUT_MS)) {
      isScreenOff = true;
      u8g2.setPowerSave(1); 
      Serial.println("Screen Auto Off (Sleep Mode)");
    }
    if (!isScreenOff) {
      ui.render();
    }
    vTaskDelayUntil(&xLastWakeTime, xFrequency);
  }
}

// ===== 输入任务 (100Hz) =====
void taskInput(void *pvParameters) {
  TickType_t xLastWakeTime = xTaskGetTickCount();
  const TickType_t xFrequency = pdMS_TO_TICKS(10); 

  int btnState = HIGH;
  int btnCounter = 0;
  unsigned long pressStartTime = 0;
  bool longPressHandled = false;

  while (1) {
    // 1. 读取编码器增量
    portENTER_CRITICAL(&mux);
    int delta = encoderDelta;
    encoderDelta = 0;
    portEXIT_CRITICAL(&mux);

    // 2. 读取SET按键（状态机去抖与长短按检测）
    bool rawState = digitalRead(BTN_SET);
    unsigned long now = millis();

    if (rawState != btnState) {
      btnCounter++;
      if (btnCounter >= 3) { // 30ms 稳定
        btnState = rawState;
        btnCounter = 0;

        if (btnState == LOW) { // 按下
          if (isScreenOff) {
            wakeUpScreen();
            longPressHandled = true; 
          } else {
            wakeUpScreen();
            pressStartTime = now;
            longPressHandled = false;
          }
        } else { // 抬起
          if (!longPressHandled && (now - pressStartTime < 1000) && (now - pressStartTime > 50)) {
            // 短按触发
            ui.processInput(0, false, true);

            if (ui.consumeToggleWifi()) {
                bool newState = !webConfig.isAPEnabled();
                webConfig.setAPEnabled(newState);
                ui.setWifiConnected(newState);
            }
            if (ui.consumeToggleBt()) {
                bool newState = !bleConfig.isBleEnabled();
                bleConfig.setBleEnabled(newState);
                fram.writeByte(FRAM_ADDR_BLE_EN, newState ? 1 : 0);
                ui.setBtConnected(newState);
            }
            if (ui.consumeToggleMode()) {
                currentWorkMode = (currentWorkMode == MODE_ESC) ? MODE_FAN : MODE_ESC;
                ui.setModeEsc(currentWorkMode == MODE_ESC);
                applyPwmConfig(ui.getProgress(), true);
                fram.writeByte(FRAM_ADDR_MODE, currentWorkMode);
                webConfig.updateStatus(currentWorkMode, ui.getProgress());
                bleConfig.updateStatus(currentWorkMode, ui.getProgress());
            }
            if (ui.consumeToggleFlip()) {
                bool newState = !ui.isFlipEnabled();
                ui.setFlipEnabled(newState);
                u8g2.setDisplayRotation(newState ? U8G2_R2 : U8G2_R0);
                fram.writeByte(FRAM_ADDR_FLIP_EN, newState ? 1 : 0);
                Serial.printf("Screen Flip: %s\n", newState ? "180" : "0");
            }
            if (ui.consumeToggleSleep()) {
                bool newState = !ui.isSleepEnabled();
                ui.setSleepEnabled(newState);
                autoSleepEnabled = newState;
                fram.writeByte(FRAM_ADDR_SLEEP_EN, newState ? 1 : 0);
                Serial.printf("Auto Sleep: %s\n", newState ? "ON" : "OFF");
            }
            if (ui.consumeBrightnessChange()) {
                int b = ui.getBrightness();
                u8g2.setContrast(b);
                uint8_t brightBuf[2] = {(uint8_t)(b & 0xFF), (uint8_t)((b >> 8) & 0xFF)};
                fram.writeBytes(FRAM_ADDR_BRIGHTNESS, brightBuf, 2);
                Serial.printf("Brightness: %d\n", b);
            }
          }
        }
      }
    } else {
      btnCounter = 0;
      if (btnState == LOW && !longPressHandled) {
        if (now - pressStartTime >= 1000) { // 长按触发
          ui.processInput(0, true);
          longPressHandled = true;
        }
      }
    }

    // 3. 处理编码器增量
    if (delta != 0) {
      if (isScreenOff) {
        wakeUpScreen(); 
      } else {
        wakeUpScreen();
        ui.processInput(delta, false);
        
        // 第一屏主面板直接调速，第二屏菜单需进入编辑模式
        if (ui.isEditMode() || ui.getCurrentScreen() == SCREEN_FAN) {
          int progress = ui.getProgress();
          applyPwmConfig(progress);
          webConfig.updateStatus(currentWorkMode, progress);
          bleConfig.updateStatus(currentWorkMode, progress);
        }
      }
    }

    vTaskDelayUntil(&xLastWakeTime, xFrequency);
  }
}

// ===== 通讯服务任务 (100Hz) =====
void taskComm(void *pvParameters) {
  TickType_t xLastWakeTime = xTaskGetTickCount();
  const TickType_t xFrequency = pdMS_TO_TICKS(10); 

  while (1) {
    webConfig.handleClient();
    bleConfig.handleTask();
    
    if (webConfig.checkAPTimer()) {
        ui.setWifiConnected(false); 
        Serial.println("WiFi AP Auto Disabled after 10 mins");
    }
    
    vTaskDelayUntil(&xLastWakeTime, xFrequency);
  }
}

// ===== INA226 传感器遥测任务 (2Hz) =====
void taskTelemetry(void *pvParameters) {
  TickType_t xLastWakeTime = xTaskGetTickCount();
  const TickType_t xFrequency = pdMS_TO_TICKS(500); 

  while (1) {
    if (ina226Ready) {
      float v = ina226.readBusVoltage();
      float i = ina226.readCurrent();
      
      ui.setVoltage(v);
      ui.setCurrent(i);
      ui.setBattVoltage(v);

      webConfig.setTelemetry(v, i);
      bleConfig.setTelemetry(v, i);
    }
    vTaskDelayUntil(&xLastWakeTime, xFrequency);
  }
}


// ==========================================
// 主程序入口
// ==========================================

void setup() {
  Serial.begin(115200);

  CLEAR_PERI_REG_MASK(USB_SERIAL_JTAG_CONF0_REG, USB_SERIAL_JTAG_DP_PULLUP);
  CLEAR_PERI_REG_MASK(USB_SERIAL_JTAG_CONF0_REG, USB_SERIAL_JTAG_USB_PAD_ENABLE);
  delay(1000);

  // ===== 1. I2C 与 传感器初始化 =====
  Wire.begin(SDA_PIN, SCL_PIN);
  Wire.setClock(400000); // Fast Mode
  if (ina226.begin(Wire)) {
    Serial.println("INA226 Init OK");
    ina226Ready = true;
  } else {
    Serial.println("INA226 Not Found");
  }

  // ===== 2. 铁电存储初始化与参数恢复 =====
  if (fram.begin(Wire)) {
    Serial.println("MB85RC16 Init OK");
    if (fram.readByte(FRAM_ADDR_MAGIC) == FRAM_MAGIC_VALUE) {
      currentWorkMode = (WorkMode)fram.readByte(FRAM_ADDR_MODE);
      // 移除速度记忆，暴力风扇高速启动危险，每次开机强制归零
      ui.setProgress(0);
      bool savedFlip = (fram.readByte(FRAM_ADDR_FLIP_EN) == 1);
      ui.setFlipEnabled(savedFlip);
      bool savedSleep = (fram.readByte(FRAM_ADDR_SLEEP_EN) == 1);
      ui.setSleepEnabled(savedSleep);
      autoSleepEnabled = savedSleep;
      int16_t savedBrightness = (int16_t)((fram.readByte(FRAM_ADDR_BRIGHTNESS + 1) << 8) | fram.readByte(FRAM_ADDR_BRIGHTNESS));
      if (savedBrightness >= 0 && savedBrightness <= 255) {
        ui.setBrightness(savedBrightness);
      }
    } else {
      // 首次使用，写入默认值和标记
      fram.writeByte(FRAM_ADDR_MODE, MODE_ESC);
      uint8_t progBuf[2] = {0, 0};
      fram.writeBytes(FRAM_ADDR_PROGRESS, progBuf, 2);
      fram.writeByte(FRAM_ADDR_BLE_EN, 0);
      fram.writeByte(FRAM_ADDR_FLIP_EN, 0);
      fram.writeByte(FRAM_ADDR_SLEEP_EN, 1); // 默认开启自动休眠
      uint8_t brightBuf[2] = {128, 0}; // 默认亮度128
      fram.writeBytes(FRAM_ADDR_BRIGHTNESS, brightBuf, 2);
      fram.writeByte(FRAM_ADDR_MAGIC, FRAM_MAGIC_VALUE);
      Serial.println("FRAM first init, defaults written");
    }
  } else {
    Serial.println("MB85RC16 Not Found, using defaults");
  }

  // ===== 3. OLED 与 UI 初始化 =====
  u8g2.setI2CAddress(0x3C << 1);
  u8g2.begin();
  u8g2.setPowerSave(0);
  // 恢复屏幕翻转状态
  if (ui.isFlipEnabled()) {
    u8g2.setDisplayRotation(U8G2_R2);
  }
  // 恢复屏幕亮度
  u8g2.setContrast(ui.getBrightness());
  ui.init(&u8g2);

  // ===== 4. PWM 初始化（必须在开机动画之前，确保电调上电立即收到最低油门信号完成自检）=====
  ui.setModeEsc(currentWorkMode == MODE_ESC);
  applyPwmConfig(ui.getProgress(), true);

  ui.drawBootScreen("v1.0"); 
  wakeUpScreen();     
  // 开机动画已在 drawBootScreen 中阻塞3秒，此处无需额外延时

  // ===== 5. 通讯模块初始化 =====
  webConfig.init();
  webConfig.updateStatus(currentWorkMode, ui.getProgress());  
  webConfig.onConfigChange([](WorkMode mode, int progress) {
    wakeUpScreen();
    currentWorkMode = mode;
    ui.setModeEsc(mode == MODE_ESC);
    ui.setProgress(progress);
    applyPwmConfig(progress, true);
    fram.writeByte(FRAM_ADDR_MODE, mode);
    bleConfig.updateStatus(mode, progress);
  });

  bleConfig.init();
  bleConfig.updateStatus(currentWorkMode, ui.getProgress());
  // 从铁电恢复蓝牙开关状态
  {
    bool savedBle = (fram.readByte(FRAM_ADDR_BLE_EN) == 1);
    if (savedBle) {
      bleConfig.setBleEnabled(true);
      ui.setBtConnected(true);
    }
  }
  bleConfig.onConfigChange([](WorkMode mode, int progress) {
    wakeUpScreen();
    currentWorkMode = mode;
    ui.setModeEsc(mode == MODE_ESC);
    ui.setProgress(progress);
    applyPwmConfig(progress, true);
    fram.writeByte(FRAM_ADDR_MODE, mode);
    webConfig.updateStatus(mode, progress);
  });

  // ===== 6. 编码器与外部中断初始化 =====
  pinMode(ENC_A, INPUT);
  pinMode(ENC_B, INPUT);
  pinMode(BTN_SET, INPUT);
  attachInterrupt(digitalPinToInterrupt(ENC_A), isrEncoder, CHANGE);
  attachInterrupt(digitalPinToInterrupt(ENC_B), isrEncoder, CHANGE);

  // ===== 7. 启动 FreeRTOS 任务 =====
  xTaskCreatePinnedToCore(taskDisplay, "Display", 4096, NULL, 1, NULL, 0);
  xTaskCreatePinnedToCore(taskInput, "Input", 2048, NULL, 2, NULL, 0);
  xTaskCreatePinnedToCore(taskComm, "Comm", 4096, NULL, 1, NULL, 0);
  xTaskCreatePinnedToCore(taskTelemetry, "Telemetry", 2048, NULL, 1, NULL, 0);

  Serial.println("System Ready");
  vTaskDelete(NULL); // 删除空的 loop 任务释放 Core 1 资源
}
void loop() {}
// small_fan.ino