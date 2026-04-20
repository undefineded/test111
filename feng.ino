#include <Arduino.h>
#include <Wire.h>
#include <U8g2lib.h>
#include <Preferences.h>
#include "DisplayUI.h"
#include "WebConfig.h"
#include "BleConfig.h"

// ===== 持久化存储 =====
Preferences prefs;

// ===== 引脚 =====
#define SDA_PIN 4
#define SCL_PIN 5

#define PWM_PIN 6

#define ENC_A 3
#define ENC_B 10
#define BTN_SET 7

// ===== OLED =====
U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2(
  U8G2_R0,
  U8X8_PIN_NONE);

// 初始化独立UI模块
DisplayUI ui;

// ===== 运行模式与全局变量 =====
WorkMode currentWorkMode = MODE_ESC;

// ===== 编码器变量 =====
volatile int encoderDelta = 0;
volatile unsigned long lastInterruptTime = 0;

// ===== 临界区锁（解决跨核并发竞态） =====
portMUX_TYPE mux = portMUX_INITIALIZER_UNLOCKED;

// ===== 休眠控制 =====
unsigned long lastActivityTime = 0;
bool isScreenOff = false;
const unsigned long SLEEP_TIMEOUT_MS = 30000; // 30秒无操作息屏

void wakeUpScreen() {
  if (isScreenOff) {
    isScreenOff = false;
    u8g2.setPowerSave(0);
    Serial.println("Screen Woke Up");
  }
  lastActivityTime = millis();
}

// ===== 动态 PWM 配置与应用 =====
void applyPwmConfig(int progress, bool forceReattach = false) {
  static WorkMode lastMode = (WorkMode)-1; // 记录上一次的模式
  bool modeChanged = (currentWorkMode != lastMode);
  
  if (currentWorkMode == MODE_ESC) {
    if (modeChanged || forceReattach) {
      ledcAttach(PWM_PIN, 50, 12); // 无刷电调：50Hz, 12位分辨率
      lastMode = currentWorkMode;
    }
    // 周期 20ms = 20000us
    // 975us = 975 / 20000 * 4096 ≈ 200
    // 1950us = 1950 / 20000 * 4096 ≈ 400
    int duty = map(progress, 0, 100, 200, 400);
    ledcWrite(PWM_PIN, duty);
  } else if (currentWorkMode == MODE_FAN) {
    if (modeChanged || forceReattach) {
      ledcAttach(PWM_PIN, 25000, 10); // 服务器风扇：25kHz, 10位分辨率
      lastMode = currentWorkMode;
    }
    int duty = map(progress, 0, 100, 0, 1023);
    ledcWrite(PWM_PIN, duty);
  }
}

// ===== 编码器中断 =====
void IRAM_ATTR isrEncoder() {
  unsigned long now = millis();

  // 去抖（5ms 过滤机械抖动）
  if (now - lastInterruptTime < 5) return;

  int a = digitalRead(ENC_A);
  int b = digitalRead(ENC_B);

  // 加锁保护跨核共享变量 encoderDelta
  portENTER_CRITICAL_ISR(&mux);
  if (a == b) {
    encoderDelta++;
  } else {
    encoderDelta--;
  }
  portEXIT_CRITICAL_ISR(&mux);

  lastInterruptTime = now;
}

// ===== UI任务 =====
void taskDisplay(void *pvParameters) {
  TickType_t xLastWakeTime = xTaskGetTickCount();
  const TickType_t xFrequency = pdMS_TO_TICKS(200); // 目标刷新率：5 FPS
  while (1) {
    if (!isScreenOff && (millis() - lastActivityTime > SLEEP_TIMEOUT_MS)) {
      isScreenOff = true;
      u8g2.setPowerSave(1); // 关闭 OLED 面板电源
      Serial.println("Screen Auto Off (Sleep Mode)");
    }

    if (!isScreenOff) {
      // 交给独立的UI模块渲染
      ui.render();
    }
    
    // 使用绝对延时，保证固定周期执行，而不是依赖于渲染耗时的相对延时
    vTaskDelayUntil(&xLastWakeTime, xFrequency);
  }
}

// ===== 输入任务 =====
void taskInput(void *pvParameters) {
  TickType_t xLastWakeTime = xTaskGetTickCount();
  const TickType_t xFrequency = pdMS_TO_TICKS(50); // 目标采样率：100 Hz

  int btnState = HIGH;
  int btnCounter = 0;
  
  unsigned long pressStartTime = 0;
  bool longPressHandled = false;

  while (1) {
    // ===== 读取编码器变化 =====
    portENTER_CRITICAL(&mux);
    int delta = encoderDelta;
    encoderDelta = 0;
    portEXIT_CRITICAL(&mux);

    // ===== 读取SET按键（状态机去抖与长按检测） =====
    bool rawState = digitalRead(BTN_SET);
    unsigned long now = millis();

    if (rawState != btnState) {
      btnCounter++;
      // 连续3次(30ms)采样稳定才确认状态变化
      if (btnCounter >= 3) {
        btnState = rawState;
        btnCounter = 0;

        if (btnState == LOW) {
          // 按下开始计时
          if (isScreenOff) {
            wakeUpScreen();
            longPressHandled = true; // 阻止休眠唤醒时的长短按触发
          } else {
            wakeUpScreen();
            pressStartTime = now;
            longPressHandled = false;
          }
        } else {
          // 抬起按键
          if (!longPressHandled && (now - pressStartTime < 1000) && (now - pressStartTime > 50)) {
            // 触发短按
            ui.processInput(0, false, true);

            // 检查 UI 是否发出了切换 WiFi AP 或蓝牙的事件
            if (ui.consumeToggleWifi()) {
                bool newState = !webConfig.isAPEnabled();
                webConfig.setAPEnabled(newState);
                ui.setWifiConnected(newState);
            }
            if (ui.consumeToggleBt()) {
                bool newState = !bleConfig.isBleEnabled();
                bleConfig.setBleEnabled(newState);
                ui.setBtConnected(newState);
            }
            if (ui.consumeToggleMode()) {
                currentWorkMode = (currentWorkMode == MODE_ESC) ? MODE_FAN : MODE_ESC;
                ui.setModeEsc(currentWorkMode == MODE_ESC);
                ui.setModeName(currentWorkMode == MODE_ESC ? "电调" : "风扇");
                applyPwmConfig(ui.getProgress(), true);
                prefs.putUInt("mode", currentWorkMode);
                webConfig.updateStatus(currentWorkMode, ui.getProgress());
                bleConfig.updateStatus(currentWorkMode, ui.getProgress());
            }
          }
        }
      }
    } else {
      btnCounter = 0;
      // 保持按下状态时，检测是否超过1秒触发长按
      if (btnState == LOW && !longPressHandled) {
        if (now - pressStartTime >= 1000) {
          // 触发长按
          ui.processInput(0, true);
          longPressHandled = true;
        }
      }
    }

    // ===== 传递编码器增量 =====
    if (delta != 0) {
      if (isScreenOff) {
        wakeUpScreen(); // 屏幕休眠时，第一次旋转仅唤醒屏幕，不调节数值
      } else {
        wakeUpScreen();
        ui.processInput(delta, false);
        
        // 当处于编辑模式时，将UI的进度同步到PWM并上报给Web/BLE状态
        if (ui.isEditMode()) {
          int progress = ui.getProgress(); // 0-100
          applyPwmConfig(progress);
          webConfig.updateStatus(currentWorkMode, progress);
          // bleConfig.updateStatus(currentWorkMode, progress);
        }
      }
    }

    vTaskDelayUntil(&xLastWakeTime, xFrequency);
  }
}

// ===== 通讯服务任务 =====
void taskComm(void *pvParameters) {
  TickType_t xLastWakeTime = xTaskGetTickCount();
  const TickType_t xFrequency = pdMS_TO_TICKS(10); // 处理网络与蓝牙事件，100Hz

  while (1) {
    webConfig.handleClient();
    // bleConfig.handleTask();
    
    // 检查 AP 10分钟超时
    if (webConfig.checkAPTimer()) {
        ui.setWifiConnected(false); // 自动关闭时同步关闭 UI 图标
        Serial.println("WiFi AP Auto Disabled after 10 mins");
    }

    // 检查 BLE 10分钟超时
    if (bleConfig.checkBleTimer()) {
        ui.setBtConnected(false);
        Serial.println("BLE Auto Disabled after 10 mins");
    }
    
    vTaskDelayUntil(&xLastWakeTime, xFrequency);
  }
}

void setup() {
  Serial.begin(115200);
  delay(1000);

  // ===== I2C =====
  Wire.begin(SDA_PIN, SCL_PIN);
  // 优化：将 I2C 时钟频率从 100kHz 提升至 400kHz (Fast Mode)
  Wire.setClock(400000);

  // ===== 读取持久化模式 =====
  prefs.begin("fan_cfg", false);
  currentWorkMode = (WorkMode)prefs.getUInt("mode", MODE_ESC);

  u8g2.setI2CAddress(0x3C << 1);
  u8g2.begin();
  u8g2.setPowerSave(0);

  // ===== UI =====
  ui.init(&u8g2);
  ui.setProgress(0);  // 安全起见，上电默认进度为0
  wakeUpScreen();     // 初始化活跃时间

  // ===== Web 网页配置 =====
  webConfig.init();
  webConfig.updateStatus(currentWorkMode, 0);  // 同步初始进度0到Web

  // 注册 Web 网页端下发的参数改变回调
  webConfig.onConfigChange([](WorkMode mode, int progress) {
    wakeUpScreen();
    currentWorkMode = mode;
    // 切换UI显示名称（防止过长溢出屏幕，缩写为“电调”和“风扇”）
    ui.setModeName(mode == MODE_ESC ? "电调" : "风扇");
    ui.setModeEsc(mode == MODE_ESC);
    ui.setProgress(progress);
    applyPwmConfig(progress, true);

    // 仅持久化保存模式
    prefs.putUInt("mode", mode);
    
    // 同步给 BLE 客户端
    bleConfig.updateStatus(mode, progress);
  });

  // ===== BLE 蓝牙配置 =====
  bleConfig.init();
  bleConfig.updateStatus(currentWorkMode, 0); // 同步初始进度0

  // 注册 BLE 端下发的参数改变回调
  bleConfig.onConfigChange([](WorkMode mode, int progress) {
    wakeUpScreen();
    currentWorkMode = mode;
    ui.setModeName(mode == MODE_ESC ? "电调" : "风扇");
    ui.setModeEsc(mode == MODE_ESC);
    ui.setProgress(progress);
    applyPwmConfig(progress, true);

    prefs.putUInt("mode", mode);
    
    // 同步给 Web 客户端
    webConfig.updateStatus(mode, progress);
  });

  // 初始化 PWM
  ui.setModeName(currentWorkMode == MODE_ESC ? "电调" : "风扇");
  ui.setModeEsc(currentWorkMode == MODE_ESC);
  applyPwmConfig(ui.getProgress());

  // ===== 编码器 =====
  pinMode(ENC_A, INPUT_PULLUP);
  pinMode(ENC_B, INPUT_PULLUP);
  pinMode(BTN_SET, INPUT_PULLUP);

  attachInterrupt(digitalPinToInterrupt(ENC_A), isrEncoder, CHANGE);

  // ===== 任务 =====
  xTaskCreatePinnedToCore(taskDisplay, "Display", 4096, NULL, 1, NULL, 0);
  xTaskCreatePinnedToCore(taskInput, "Input", 2048, NULL, 2, NULL, 0);
  xTaskCreatePinnedToCore(taskComm, "Comm", 4096, NULL, 1, NULL, 0);

  Serial.println("Encoder & WebConfig & BLE Version Ready");

  // 彻底删除空的 loop 任务释放 Core 1 资源
  vTaskDelete(NULL);
}

void loop() {}

// small_fan.ino