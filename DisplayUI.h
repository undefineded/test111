#ifndef DISPLAY_UI_H
#define DISPLAY_UI_H
#include <Arduino.h>
#include <U8g2lib.h>

// 屏幕状态枚举
enum UIScreen {
    SCREEN_FAN = 0,
    SCREEN_MENU = 1,
    SCREEN_INFO = 2
};

// 菜单焦点项枚举
enum FocusItem {
    MENU_SPEED = 0,
    MENU_MODE = 1,
    MENU_BT = 2,
    MENU_WIFI = 3,
    MENU_FLIP = 4,
    MENU_SLEEP = 5,
    MENU_BRIGHTNESS = 6
};

class DisplayUI {
public:
    // 初始化，传入U8G2对象指针
    void init(U8G2 *u8g2_ptr);
    
    // 渲染当前屏幕
    void render();
    
    // 处理输入事件：编码器增量和长/短按按键
    void processInput(int encoderDelta, bool isLongPress, bool isShortPress = false);

    // 暴露是否发生了状态切换动作供主程序消费
    bool consumeToggleWifi() { bool v = flagToggleWifi; flagToggleWifi = false; return v; }
    bool consumeToggleBt() { bool v = flagToggleBt; flagToggleBt = false; return v; }
    bool consumeToggleMode() { bool v = flagToggleMode; flagToggleMode = false; return v; }
    bool consumeToggleFlip() { bool v = flagToggleFlip; flagToggleFlip = false; return v; }
    bool consumeToggleSleep() { bool v = flagToggleSleep; flagToggleSleep = false; return v; }
    bool consumeBrightnessChange() { bool v = flagBrightnessChange; flagBrightnessChange = false; return v; }
    
    // 获取蓝牙和WiFi的当前状态
    bool getBtState() const { return btConnected; }
    bool getWifiState() const { return wifiConnected; }
    bool isModeEsc() const { return modeEsc; }

    // 获取/设置进度（0-100%）
    int getProgress() const { return progress; }
    void setProgress(int p) {
        progress = p;
        if (progress > 100) progress = 100;
        if (progress < 0) progress = 0;
    }
    
    // 设置电压、电流，自动计算功率
    void setVoltage(float v) { voltage = v; }
    void setCurrent(float c) { current = c; }
    void setBattVoltage(float v) { battVoltage = v; }
    void setBtConnected(bool c) { btConnected = c; }
    void setWifiConnected(bool c) { wifiConnected = c; }
    void setModeEsc(bool e) { modeEsc = e; tempModeEsc = e; }
    void setFlipEnabled(bool f) { flipEnabled = f; }
    void setSleepEnabled(bool s) { sleepEnabled = s; }
    bool isFlipEnabled() const { return flipEnabled; }
    bool isSleepEnabled() const { return sleepEnabled; }

    // 获取/设置亮度（0-255）
    int getBrightness() const { return brightness; }
    void setBrightness(int b) {
        brightness = b;
        if (brightness > 255) brightness = 255;
        if (brightness < 0) brightness = 0;
    }

    // 获取当前是否处于编辑模式
    bool isEditMode() const { return isEditing; }
    // 获取当前屏幕
    UIScreen getCurrentScreen() const { return currentScreen; }

    // 绘制开机画面
    void drawBootScreen(const char* version = "v1.0");

private:
    static const int TOTAL_MENU_ITEMS = 7;
    static const int VISIBLE_MENU_ROWS = 3;

    U8G2 *u8g2;
    UIScreen currentScreen = SCREEN_FAN;
    bool isEditing = false;

    // UI数据
    int progress = 0;
    float voltage = 0;
    float current = 0;
    float battVoltage = 0;
    bool btConnected = false; // 默认关闭
    bool wifiConnected = false; // 默认关闭

    FocusItem currentFocus = MENU_SPEED;
    int firstVisibleIndex = 0;
    bool flagToggleWifi = false;
    bool flagToggleBt = false;
    bool flagToggleMode = false;
    bool flagToggleFlip = false;
    bool flagToggleSleep = false;
    bool flagBrightnessChange = false;
    unsigned long lastMenuSwitchTime = 0;
    static const unsigned long MENU_SWITCH_THROTTLE_MS = 120;
    unsigned long lastSpeedAdjustTime = 0;
    static const unsigned long SPEED_ADJUST_THROTTLE_MS = 70;
    unsigned long lastModeToggleTime = 0;
    static const unsigned long MODE_TOGGLE_THROTTLE_MS = 150;

    // 编码器加速步进状态
    int encoderAccel = 0;              // 加速计数器，连续快速转动时累加
    unsigned long lastEncoderTickTime = 0; // 上次旋钮脉冲时间
    static const unsigned long FAST_TICK_MS = 120;  // 快速转动判定阈值
    static const unsigned long IDLE_TICK_MS = 250;   // 停转复位阈值
    static const int MAX_ACCEL_STEP = 10;             // 最大步进值

    // 双击紧急停止状态
    unsigned long lastShortPressTime = 0;               // 上次短按时间
    static const unsigned long DOUBLE_CLICK_MS = 300;   // 双击判定阈值

    // 编辑模式下的临时状态（用于旋转时的预览，确认后触发真实切换）
    bool tempBtState = false;
    bool tempWifiState = false;
    bool modeEsc = true;
    bool tempModeEsc = true;
    bool flipEnabled = false;
    bool tempFlipEnabled = false;
    bool sleepEnabled = true;
    bool tempSleepEnabled = true;
    int brightness = 128;       // 亮度 0-255，默认128
    int tempBrightness = 128;   // 编辑模式下的临时亮度

    // 风扇主面板动画状态
    int displayProgress = 0;        // 进度条动画用的显示值
    int fanAnimPhase = 0;           // 风扇图标旋转阶段
    unsigned long lastFanAnimTime = 0; // 上次风扇动画更新时间
    float fanAnimSpeed = 0;         // 当前动画帧率(fps)，平滑过渡
    
    // 局部刷新状态记录
    bool forceFullRefresh = true;
    float lastDrawVoltage = -1.0f;
    float lastDrawCurrent = -1.0f;
    int lastDrawProgress = -1;
    int lastFanAnimPhase = -1;
    
    // 内部绘制函数
    void drawScreen0();
    void drawScreen1();
    void drawScreen2();
};

#endif // DISPLAY_UI_H