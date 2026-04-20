#ifndef DISPLAY_UI_H
#define DISPLAY_UI_H
#include <Arduino.h>
#include <U8g2lib.h>

// 菜单焦点项枚举
enum FocusItem {
    MENU_SPEED = 0,
    MENU_MODE = 1,
    MENU_BT = 2,
    MENU_WIFI = 3
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
    void setBattery(int b) { battery = b; }
    void setBattVoltage(float v) { battVoltage = v; }
    void setTemperature(float t) { temperature = t; }
    void setBtConnected(bool c) { btConnected = c; }
    void setWifiConnected(bool c) { wifiConnected = c; }
    void setModeEsc(bool esc) { modeEsc = esc; }

    // 动态设置模式名称
    void setModeName(const char* name) { modeName = name; }

    // 获取当前是否处于编辑模式
    bool isEditMode() const { return isEditing; }

private:
    static const int TOTAL_MENU_ITEMS = 4;
    static const int VISIBLE_MENU_ROWS = 3;

    U8G2 *u8g2;
    bool isEditing = false;
    const char* modeName = "风扇";

    // UI数据
    int progress = 0;
    float voltage = 12.22;
    float current = 1.20;
    int battery = 100;
    float battVoltage = 12.4;
    float temperature = 35.5;
    bool btConnected = false; // 默认关闭
    bool wifiConnected = false; // 默认关闭

    FocusItem currentFocus = MENU_SPEED;
    int firstVisibleIndex = 0;
    bool flagToggleWifi = false;
    bool flagToggleBt = false;
    bool flagToggleMode = false;
    unsigned long lastMenuSwitchTime = 0;
    static const unsigned long MENU_SWITCH_THROTTLE_MS = 120;

    // 编辑模式下的临时状态（用于旋转时的预览，确认后触发真实切换）
    bool tempBtState = false;
    bool tempWifiState = false;
    bool modeEsc = true;
    bool tempModeEsc = true;
};

#endif // DISPLAY_UI_H