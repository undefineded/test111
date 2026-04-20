#ifndef BLE_CONFIG_H
#define BLE_CONFIG_H

#include <Arduino.h>
#include "WebConfig.h" // 引用 WorkMode 枚举

class BleConfig {
public:
    // 初始化并建立 BLE 服务
    void init();
    
    // 回调函数类型，当手机 App 通过 BLE 修改配置时触发
    typedef void (*ConfigChangeCallback)(WorkMode mode, int progress);
    
    // 注册回调
    void onConfigChange(ConfigChangeCallback cb) { callback = cb; }

    // 主动同步设备状态到手机 App（如：通过旋钮调节后）
    void updateStatus(WorkMode mode, int progress);

    // 动态控制 BLE 开关与 10 分钟自动关闭
    void setBleEnabled(bool enable);
    bool isBleEnabled() const { return bleEnabled; }
    bool checkBleTimer(); // 在主循环中调用，超时返回true

    // 更新采集数据
    void setTelemetry(float v, float i) {
        voltage = v;
        current = i;
        notifyStatus(); // 每次采集后如果需要可通知，这里暂定
    }

    // 内部供回调使用的触发器
    void triggerCallback(WorkMode m, int p) {
        if (callback) callback(m, p);
    }

    void setDeviceConnected(bool connected) {
        deviceConnected = connected;
    }

private:
    void notifyStatus();

    float voltage = 12.22; // 默认模拟值
    float current = 1.20;  // 默认模拟值
    WorkMode currentMode = MODE_ESC;
    int currentProgress = 0;
    ConfigChangeCallback callback = nullptr;

    bool bleEnabled = false;
    bool deviceConnected = false;
    unsigned long bleStartTime = 0;
};

extern BleConfig bleConfig;

#endif // BLE_CONFIG_H