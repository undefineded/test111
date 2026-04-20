#include "BleConfig.h"
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>

BleConfig bleConfig;

#define SERVICE_UUID           "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
#define CHARACTERISTIC_UUID_RX "beb5483e-36e1-4688-b7f5-ea07361b26a8" // App 发送给 ESP32 (Write)
#define CHARACTERISTIC_UUID_TX "4c8c4a09-cc86-4e5b-1188-662e08cc8957" // ESP32 发送给 App (Notify)

static BLEServer *pServer = nullptr;
static BLECharacteristic * pTxCharacteristic = nullptr;

class MyServerCallbacks: public BLEServerCallbacks {
    void onConnect(BLEServer* pServer) {
        bleConfig.setDeviceConnected(true);
        Serial.println("BLE Device Connected");
    }
    void onDisconnect(BLEServer* pServer) {
        bleConfig.setDeviceConnected(false);
        Serial.println("BLE Device Disconnected");
        // 断开连接后重新开始广播，以便其他设备连接
        if (bleConfig.isBleEnabled()) {
            BLEDevice::startAdvertising();
        }
    }
};

class MyCallbacks: public BLECharacteristicCallbacks {
    void onWrite(BLECharacteristic *pCharacteristic) {
        String rxValue = pCharacteristic->getValue().c_str();
        if (rxValue.length() > 0) {
            // 解析来自 App 的指令，支持格式: M=0&P=50 (M: 模式 0/1, P: 进度 0-100)
            int m = -1, p = -1;
            if (sscanf(rxValue.c_str(), "M=%d&P=%d", &m, &p) == 2) {
                bleConfig.triggerCallback((WorkMode)m, p);
                Serial.printf("BLE Recv Config -> Mode: %d, Prog: %d\n", m, p);
            }
        }
    }
};

void BleConfig::init() {
    BLEDevice::init("ESP32_PWM_BLE");
    
    pServer = BLEDevice::createServer();
    pServer->setCallbacks(new MyServerCallbacks());

    BLEService *pService = pServer->createService(SERVICE_UUID);

    // TX 特征（ESP32 到 App 的 Notify）
    pTxCharacteristic = pService->createCharacteristic(
        CHARACTERISTIC_UUID_TX,
        BLECharacteristic::PROPERTY_NOTIFY
    );
    pTxCharacteristic->addDescriptor(new BLE2902());

    // RX 特征（App 到 ESP32 的 Write）
    BLECharacteristic * pRxCharacteristic = pService->createCharacteristic(
        CHARACTERISTIC_UUID_RX,
        BLECharacteristic::PROPERTY_WRITE
    );
    pRxCharacteristic->setCallbacks(new MyCallbacks());

    pService->start();
    
    // 初始化完成默认开启 BLE 广播
    setBleEnabled(true);
}

void BleConfig::setBleEnabled(bool enable) {
    bleEnabled = enable;
    if (enable) {
        BLEDevice::startAdvertising();
        bleStartTime = millis();
        Serial.println("BLE Enabled and Advertising");
    } else {
        // 关闭广播（不再接受新连接）
        BLEDevice::stopAdvertising();
        Serial.println("BLE Disabled");
    }
}

bool BleConfig::checkBleTimer() {
    // 开启后超过 10 分钟自动关闭 BLE
    if (bleEnabled && (millis() - bleStartTime > 10 * 60 * 1000)) {
        setBleEnabled(false);
        return true; // 触发了自动关闭
    }
    return false;
}

void BleConfig::updateStatus(WorkMode mode, int progress) {
    currentMode = mode;
    currentProgress = progress;
    notifyStatus(); // 状态改变时通知 App
}

void BleConfig::notifyStatus() {
    // 将状态打包成 JSON 通过 Notify 发送给连接的手机 App
    if (pTxCharacteristic != nullptr) {
        char buf[128];
        snprintf(buf, sizeof(buf), "{\"mode\":%d,\"progress\":%d,\"v\":%.2f,\"i\":%.2f}", 
                 currentMode, currentProgress, voltage, current);
        pTxCharacteristic->setValue((uint8_t*)buf, strlen(buf));
        pTxCharacteristic->notify();
    }
}
