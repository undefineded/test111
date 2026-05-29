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
        if (bleConfig.isBleEnabled()) {
            BLEDevice::startAdvertising();
        }
    }
};

class MyCallbacks: public BLECharacteristicCallbacks {
    void onWrite(BLECharacteristic *pCharacteristic) {
        String rxValue = pCharacteristic->getValue().c_str();
        if (rxValue.length() > 0) {
            Serial.printf("BLE Recv: %s\n", rxValue.c_str());
            if (rxValue == "HBT") {
                bleConfig.requestStatus();
            } else {
                int m = -1, p = -1;
                if (sscanf(rxValue.c_str(), "M=%d&P=%d", &m, &p) == 2) {
                    bleConfig.triggerCallback((WorkMode)m, p);
                    Serial.printf("BLE Recv Config -> Mode: %d, Prog: %d\n", m, p);
                }
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
        BLECharacteristic::PROPERTY_NOTIFY | BLECharacteristic::PROPERTY_READ
    );
    pTxCharacteristic->addDescriptor(new BLE2902());

    // RX 特征（App 到 ESP32 的 Write）
    BLECharacteristic * pRxCharacteristic = pService->createCharacteristic(
        CHARACTERISTIC_UUID_RX,
        BLECharacteristic::PROPERTY_WRITE
    );
    pRxCharacteristic->setCallbacks(new MyCallbacks());

    pService->start();
    
    // 初始化完成默认关闭 BLE 广播
    setBleEnabled(false);
}

void BleConfig::setBleEnabled(bool enable) {
    bleEnabled = enable;
    if (enable) {
        BLEDevice::startAdvertising();
        Serial.println("BLE Enabled and Advertising");
    } else {
        BLEDevice::stopAdvertising();
        Serial.println("BLE Disabled");
    }
}

void BleConfig::updateStatus(WorkMode mode, int progress) {
    currentMode = mode;
    currentProgress = progress;
    pendingNotify = true;
}

void BleConfig::requestStatus() {
    notifyStatus();
}

void BleConfig::handleTask() {
    if (pendingNotify && (millis() - lastNotifyTime >= 500)) {
        pendingNotify = false;
        lastNotifyTime = millis();
        lastTelemetryNotifyTime = millis();
        notifyStatus();
    }
    if (!pendingNotify && deviceConnected && (millis() - lastTelemetryNotifyTime >= 3000)) {
        lastTelemetryNotifyTime = millis();
        notifyStatus();
    }
}

void BleConfig::notifyStatus() {
    if (!deviceConnected) return;
    if (pTxCharacteristic == nullptr) return;
    
    char buf[64];
    snprintf(buf, sizeof(buf), "%d,%d,%.2f,%.2f", 
             currentMode, currentProgress, voltage, current);
    pTxCharacteristic->setValue((uint8_t*)buf, strlen(buf));
    pTxCharacteristic->notify();
    Serial.printf("BLE Notify: %s\n", buf);
}
// bleconfig.cpp
