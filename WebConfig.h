
#ifndef WEB_CONFIG_H
#define WEB_CONFIG_H

#include <Arduino.h>
#include <DNSServer.h>

// 运行模式枚举
enum WorkMode {
    MODE_ESC = 0, // 无刷电调
    MODE_FAN = 1  // 服务器风扇
};

class WebConfig {
public:
    // 初始化并建立 AP 热点
    void init();
    
    // 放入任务中循环处理客户端请求
    void handleClient();
    
    // 回调函数类型，当网页端配置发生变化时触发
    typedef void (*ConfigChangeCallback)(WorkMode mode, int progress);
    
    // 注册回调
    void onConfigChange(ConfigChangeCallback cb) { callback = cb; }

    // 主动同步设备状态到网页端（如：通过旋钮调节后）
    void updateStatus(WorkMode mode, int progress);

    // 动态控制 AP 开关与 10 分钟自动关闭
    void setAPEnabled(bool enable);
    bool isAPEnabled() const { return apTargetEnabled; }
    bool checkAPTimer(); // 在主循环中调用，超时返回true

    // 更新采集数据（供未来 INA226 采集后调用）
    void setTelemetry(float v, float i) {
        voltage = v;
        current = i;
    }

private:
    void applyPendingAPChange();

    float voltage = 12.22; // 默认模拟值
    float current = 1.20;  // 默认模拟值
    WorkMode currentMode = MODE_ESC;
    int currentProgress = 0;
    ConfigChangeCallback callback = nullptr;

    bool apEnabled = false;
    bool apTargetEnabled = false;
    bool apChangePending = false;
    unsigned long apStartTime = 0;
};

extern WebConfig webConfig;

#endif // WEB_CONFIG_H