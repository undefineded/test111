import Taro from '@tarojs/taro'

export interface BleDevice {
  deviceId: string
  name: string
  RSSI: number
  connected: boolean
}

export interface BleStatus {
  mode: number
  progress: number
  v: number
  i: number
}

export const BLE_CONFIG = {
  serviceUUID: '4fafc201-1fb5-459e-8fcc-c5c9c331914b',
  writeCharacteristicUUID: 'beb5483e-36e1-4688-b7f5-ea07361b26a8',
  notifyCharacteristicUUID: '4c8c4a09-cc86-4e5b-1188-662e08cc8957'
}

class BleManager {
  private discoveredDevices: Map<string, BleDevice> = new Map()
  private connectedDeviceId: string = ''
  private isScanning: boolean = false
  private statusCallback: ((status: BleStatus) => void) | null = null

  async initBluetooth(): Promise<boolean> {
    try {
      try { await Taro.closeBluetoothAdapter() } catch {}
      await Taro.openBluetoothAdapter()
      return true
    } catch (error) {
      console.error('蓝牙初始化失败:', error)
      Taro.showToast({ title: '请开启蓝牙', icon: 'none' })
      return false
    }
  }

  async startScan(): Promise<void> {
    if (this.isScanning) return

    try {
      const success = await this.initBluetooth()
      if (!success) return

      this.discoveredDevices.clear()
      this.isScanning = true

      Taro.onBluetoothDeviceFound((res) => {
        res.devices.forEach(device => {
          if (device.name && device.name.length > 0) {
            this.discoveredDevices.set(device.deviceId, {
              deviceId: device.deviceId,
              name: device.name,
              RSSI: device.RSSI,
              connected: false
            })
          }
        })
      })

      await Taro.startBluetoothDevicesDiscovery({
        allowDuplicatesKey: false
      })
    } catch (error) {
      console.error('开始扫描失败:', error)
      this.isScanning = false
    }
  }

  async stopScan(): Promise<void> {
    if (!this.isScanning) return

    try {
      await Taro.stopBluetoothDevicesDiscovery()
      Taro.offBluetoothDeviceFound()
      this.isScanning = false
    } catch (error) {
      console.error('停止扫描失败:', error)
    }
  }

  getDiscoveredDevices(): BleDevice[] {
    return Array.from(this.discoveredDevices.values())
  }

  async connect(deviceId: string): Promise<boolean> {
    try {
      const success = await this.initBluetooth()
      if (!success) return false

      await Taro.createBLEConnection({ deviceId })
      return await this.setupConnection(deviceId)
    } catch (error) {
      console.error('直接连接失败，尝试扫描后重连:', error)
      return await this.connectWithScan(deviceId)
    }
  }

  async connectWithScan(deviceId: string, scanTimeout: number = 8000): Promise<boolean> {
    try {
      const success = await this.initBluetooth()
      if (!success) return false

      if (this.isScanning) {
        await this.stopScan()
      }

      this.discoveredDevices.clear()

      let found = false

      Taro.onBluetoothDeviceFound((res) => {
        res.devices.forEach(device => {
          if (device.name && device.name.length > 0) {
            this.discoveredDevices.set(device.deviceId, {
              deviceId: device.deviceId,
              name: device.name,
              RSSI: device.RSSI,
              connected: false
            })
          }
          if (device.deviceId === deviceId) {
            found = true
          }
        })
      })

      await Taro.startBluetoothDevicesDiscovery({ allowDuplicatesKey: false })

      await new Promise<void>((resolve) => {
        const checkInterval = setInterval(() => {
          if (found) {
            clearInterval(checkInterval)
            resolve()
          }
        }, 300)

        setTimeout(() => {
          clearInterval(checkInterval)
          resolve()
        }, scanTimeout)
      })

      await this.stopScan()

      if (!found) {
        console.error('扫描超时，未发现设备:', deviceId)
        return false
      }

      await Taro.createBLEConnection({ deviceId })
      return await this.setupConnection(deviceId)
    } catch (error) {
      console.error('扫描重连失败:', error)
      try { await this.stopScan() } catch {}
      return false
    }
  }

  private async requestMtu(deviceId: string): Promise<void> {
    try {
      await Taro.setBLEMTU({ deviceId, mtu: 512 })
      console.log('MTU协商成功')
    } catch (e) {
      console.warn('MTU协商失败(不影响使用):', e)
    }
  }

  private async setupConnection(deviceId: string): Promise<boolean> {
    try {
      this.connectedDeviceId = deviceId

      const device = this.discoveredDevices.get(deviceId)
      if (device) {
        device.connected = true
      }

      await new Promise(resolve => setTimeout(resolve, 600))

      await this.requestMtu(deviceId)

      const allServices = await Taro.getBLEDeviceServices({ deviceId })
      console.log('发现服务:', JSON.stringify(allServices.services?.map(s => s.uuid)))

      const targetService = allServices.services?.find(
        s => s.uuid.toLowerCase() === BLE_CONFIG.serviceUUID.toLowerCase()
      )
      if (!targetService) {
        console.error('未找到目标服务:', BLE_CONFIG.serviceUUID)
        this.connectedDeviceId = ''
        return false
      }
      console.log('找到目标服务:', targetService.uuid)

      const chars = await Taro.getBLEDeviceCharacteristics({
        deviceId,
        serviceId: BLE_CONFIG.serviceUUID
      })
      console.log('发现特征值:', JSON.stringify(chars.characteristics?.map(c => ({
        uuid: c.uuid,
        properties: c.properties
      }))))

      const notifyChar = chars.characteristics?.find(
        c => c.uuid.toLowerCase() === BLE_CONFIG.notifyCharacteristicUUID.toLowerCase()
      )
      if (notifyChar) {
        console.log('找到notify特征值:', notifyChar.uuid, 'properties:', JSON.stringify(notifyChar.properties))
      } else {
        console.warn('未找到notify特征值，已发现的特征:', chars.characteristics?.map(c => c.uuid))
      }

      const writeChar = chars.characteristics?.find(
        c => c.uuid.toLowerCase() === BLE_CONFIG.writeCharacteristicUUID.toLowerCase()
      )
      if (writeChar) {
        console.log('找到write特征值:', writeChar.uuid, 'properties:', JSON.stringify(writeChar.properties))
      } else {
        console.warn('未找到write特征值')
      }

      const targetCharId = notifyChar?.uuid || BLE_CONFIG.notifyCharacteristicUUID

      Taro.offBLECharacteristicValueChange()
      Taro.onBLECharacteristicValueChange((res) => {
        console.log('onBLECharacteristicValueChange触发, serviceId:', res.serviceId, 'characteristicId:', res.characteristicId)
        this.handleCharacteristicValueChange(res)
      })

      for (let retry = 0; retry < 3; retry++) {
        try {
          await Taro.notifyBLECharacteristicValueChange({
            deviceId,
            serviceId: BLE_CONFIG.serviceUUID,
            characteristicId: targetCharId,
            state: true
          })
          console.log('notify订阅成功, characteristicId:', targetCharId)
          break
        } catch (e) {
          console.warn(`notify订阅第${retry + 1}次失败:`, e)
          if (retry < 2) {
            await new Promise(resolve => setTimeout(resolve, 300))
          }
        }
      }

      try {
        await Taro.readBLECharacteristicValue({
          deviceId,
          serviceId: BLE_CONFIG.serviceUUID,
          characteristicId: targetCharId
        })
        console.log('主动读取特征值成功(触发onBLECharacteristicValueChange)')
      } catch (e) {
        console.warn('主动读取特征值失败:', e)
      }

      return true
    } catch (error) {
      console.error('连接设置失败:', error)
      this.connectedDeviceId = ''
      return false
    }
  }

  async disconnect(deviceId?: string): Promise<void> {
    const targetId = deviceId || this.connectedDeviceId
    if (!targetId) return

    try {
      await Taro.closeBLEConnection({ deviceId: targetId })
      const device = this.discoveredDevices.get(targetId)
      if (device) {
        device.connected = false
      }
      if (targetId === this.connectedDeviceId) {
        this.connectedDeviceId = ''
      }
    } catch (error) {
      console.error('断开连接失败:', error)
    }
  }

  async sendData(data: string): Promise<boolean> {
    if (!this.connectedDeviceId) {
      console.error('未连接设备')
      return false
    }

    try {
      const buffer = this.stringToArrayBuffer(data)
      await Taro.writeBLECharacteristicValue({
        deviceId: this.connectedDeviceId,
        serviceId: BLE_CONFIG.serviceUUID,
        characteristicId: BLE_CONFIG.writeCharacteristicUUID,
        value: buffer
      })
      return true
    } catch (error) {
      console.error('发送数据失败:', error)
      return false
    }
  }

  async sendHeartbeat(): Promise<boolean> {
    return this.sendData('HBT')
  }

  onStatusChange(callback: ((status: BleStatus) => void) | null): void {
    this.statusCallback = callback
  }

  async readStatus(): Promise<boolean> {
    if (!this.connectedDeviceId) return false
    try {
      await Taro.readBLECharacteristicValue({
        deviceId: this.connectedDeviceId,
        serviceId: BLE_CONFIG.serviceUUID,
        characteristicId: BLE_CONFIG.notifyCharacteristicUUID
      })
      return true
    } catch (error) {
      console.error('主动读取BLE状态失败:', error)
      return false
    }
  }

  private handleCharacteristicValueChange(res: any): void {
    try {
      const data = this.arrayBufferToString(res.value)
      console.log('收到BLE数据:', data, '长度:', data.length, 'deviceId:', res.deviceId)
      const status = this.parseStatusData(data)
      if (status) {
        console.log('解析成功:', JSON.stringify(status))
        if (this.statusCallback) {
          this.statusCallback(status)
        } else {
          console.warn('statusCallback未设置')
        }
      } else {
        console.warn('数据解析失败, 原始数据:', data)
      }
    } catch (error) {
      console.error('处理BLE数据异常:', error)
    }
  }

  private parseStatusData(data: string): BleStatus | null {
    const trimmed = data.trim()

    const commaParts = trimmed.split(',')
    if (commaParts.length >= 4) {
      const mode = parseInt(commaParts[0])
      const progress = parseInt(commaParts[1])
      const v = parseFloat(commaParts[2])
      const i = parseFloat(commaParts[3])
      if (!isNaN(mode) && !isNaN(progress) && !isNaN(v) && !isNaN(i)) {
        return { mode, progress, v, i }
      }
    }

    try {
      const json = JSON.parse(trimmed)
      if (json.mode !== undefined && json.progress !== undefined) {
        return {
          mode: json.mode,
          progress: json.progress,
          v: json.v || 0,
          i: json.i || 0
        }
      }
    } catch {}

    return null
  }

  private stringToArrayBuffer(str: string): ArrayBuffer {
    const buffer = new ArrayBuffer(str.length)
    const view = new Uint8Array(buffer)
    for (let i = 0; i < str.length; i++) {
      view[i] = str.charCodeAt(i)
    }
    return buffer
  }

  private arrayBufferToString(buffer: ArrayBuffer): string {
    const bytes = new Uint8Array(buffer)
    let result = ''
    for (let i = 0; i < bytes.length; i++) {
      result += String.fromCharCode(bytes[i])
    }
    return result
  }

  async refreshNotifySubscription(): Promise<boolean> {
    if (!this.connectedDeviceId) return false
    try {
      const deviceId = this.connectedDeviceId

      await this.requestMtu(deviceId)

      const allServices = await Taro.getBLEDeviceServices({ deviceId })
      console.log('刷新订阅 - 发现服务:', JSON.stringify(allServices.services?.map(s => s.uuid)))

      const chars = await Taro.getBLEDeviceCharacteristics({
        deviceId,
        serviceId: BLE_CONFIG.serviceUUID
      })
      console.log('刷新订阅 - 发现特征值:', JSON.stringify(chars.characteristics?.map(c => ({
        uuid: c.uuid,
        properties: c.properties
      }))))

      const notifyChar = chars.characteristics?.find(
        c => c.uuid.toLowerCase() === BLE_CONFIG.notifyCharacteristicUUID.toLowerCase()
      )
      const targetCharId = notifyChar?.uuid || BLE_CONFIG.notifyCharacteristicUUID

      Taro.offBLECharacteristicValueChange()
      Taro.onBLECharacteristicValueChange((res) => {
        console.log('onBLECharacteristicValueChange触发, serviceId:', res.serviceId, 'characteristicId:', res.characteristicId)
        this.handleCharacteristicValueChange(res)
      })

      for (let retry = 0; retry < 3; retry++) {
        try {
          await Taro.notifyBLECharacteristicValueChange({
            deviceId,
            serviceId: BLE_CONFIG.serviceUUID,
            characteristicId: targetCharId,
            state: true
          })
          console.log('notify刷新订阅成功, characteristicId:', targetCharId)
          break
        } catch (e) {
          console.warn(`notify刷新订阅第${retry + 1}次失败:`, e)
          if (retry < 2) {
            await new Promise(resolve => setTimeout(resolve, 300))
          }
        }
      }

      try {
        await Taro.readBLECharacteristicValue({
          deviceId,
          serviceId: BLE_CONFIG.serviceUUID,
          characteristicId: targetCharId
        })
        console.log('刷新订阅后主动读取特征值成功')
      } catch (e) {
        console.warn('刷新订阅后主动读取失败:', e)
      }

      return true
    } catch (error) {
      console.error('刷新notify订阅失败:', error)
      return false
    }
  }

  isConnected(): boolean {
    return this.connectedDeviceId !== ''
  }

  getConnectedDeviceId(): string {
    return this.connectedDeviceId
  }
}

export const bleManager = new BleManager()
