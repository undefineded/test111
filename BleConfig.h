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

    // 心跳请求：立即回传当前状态，不修改任何参数
    void requestStatus();

    // 处理定时任务（如限流发送 notify）
    void handleTask();

    // 动态控制 BLE 开关（状态持久化由主程序通过铁电存储管理）
    void setBleEnabled(bool enable);
    bool isBleEnabled() const { return bleEnabled; }

    // 更新采集数据
    void setTelemetry(float v, float i) {
        voltage = v;
        current = i;
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
    
    bool pendingNotify = false;
    unsigned long lastNotifyTime = 0;
    unsigned long lastTelemetryNotifyTime = 0;
};

extern BleConfig bleConfig;

#endif // BLE_CONFIG_H