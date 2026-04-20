#include "WebConfig.h"
#include <WiFi.h>
#include <WebServer.h>
#include "WebPage.h"

const byte DNS_PORT = 53;
DNSServer dnsServer;
WebServer server(80);
WebConfig webConfig;

// ======================== 核心逻辑 ========================

void WebConfig::setAPEnabled(bool enable) {
    apTargetEnabled = enable;
    apChangePending = true;
}

bool WebConfig::checkAPTimer() {
    if (apEnabled && (millis() - apStartTime > 10 * 60 * 1000)) {
        setAPEnabled(false);
        return true; // 触发了自动关闭
    }
    return false;
}

void WebConfig::init() {
    // 开启 AP 模式
    WiFi.mode(WIFI_AP);
    setAPEnabled(false);

    Serial.print("AP IP Address: ");
    Serial.println(WiFi.softAPIP());

    // 路由：根目录返回 HTML 页面
    server.on("/", HTTP_GET, []() {
        server.send(200, "text/html", htmlPage);
    });

    // 路由：处理 Android/iOS/Windows 等系统用来检测网络连通性的探针请求
    // 当手机连上没有互联网的WiFi时，会在后台偷偷请求下面这些地址。
    // 我们只要对这些请求返回配置页面，手机就会以为这是一个“需要登录认证”的商业WiFi，从而自动弹窗。
    server.on("/generate_204", []() { server.send(200, "text/html", htmlPage); });  // Android
    server.on("/fwlink", []() { server.send(200, "text/html", htmlPage); });      // Windows
    server.on("/hotspot-detect.html", []() { server.send(200, "text/html", htmlPage); }); // Apple
    
    // 如果用户在弹窗浏览器里乱输地址，兜底全部重定向回根目录
    server.onNotFound([]() {
        server.sendHeader("Location", "/", true);
        server.send(302, "text/plain", "");
    });

    // 路由：获取当前设备状态 (返回 JSON)
    server.on("/status", HTTP_GET, [this]() {
        char buf[128];
        snprintf(buf, sizeof(buf), "{\"mode\":%d,\"progress\":%d,\"v\":%.2f,\"i\":%.2f}", 
                 this->currentMode, this->currentProgress, this->voltage, this->current);
        server.send(200, "application/json", buf);
    });

    // 路由：设置参数并触发回调
    server.on("/set", HTTP_GET, [this]() {
        if (server.hasArg("mode") && server.hasArg("progress")) {
            this->currentMode = (WorkMode)server.arg("mode").toInt();
            this->currentProgress = server.arg("progress").toInt();
            
            // 触发外部回调
            if (this->callback) {
                this->callback(this->currentMode, this->currentProgress);
            }
            
            server.send(200, "text/plain", "OK");
        } else {
            server.send(400, "text/plain", "Bad Request");
        }
    });

    // 启动服务器
    server.begin();
    Serial.println("HTTP server started");
}

void WebConfig::handleClient() {
    applyPendingAPChange();
    dnsServer.processNextRequest();
    server.handleClient();
}

void WebConfig::updateStatus(WorkMode mode, int progress) {
    currentMode = mode;
    currentProgress = progress;
}

void WebConfig::applyPendingAPChange() {
    if (!apChangePending) return;
    apChangePending = false;

    if (apTargetEnabled == apEnabled) return;

    if (apTargetEnabled) {
        WiFi.mode(WIFI_AP);
        if (WiFi.softAP("ESP32_PWM_Config")) {
            apEnabled = true;
            apStartTime = millis();
            dnsServer.start(DNS_PORT, "*", WiFi.softAPIP());
            Serial.print("WiFi AP Enabled, IP: ");
            Serial.println(WiFi.softAPIP());
        } else {
            apEnabled = false;
            Serial.println("WiFi AP Enable Failed");
        }
    } else {
        dnsServer.stop();
        WiFi.softAPdisconnect(true);
        apEnabled = false;
        Serial.println("WiFi AP Disabled");
    }
}

//  WebConfig.cpp