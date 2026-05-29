<template>
  <view class="page">
    <view class="header-bg">
      <view class="header-content">
        <view class="device-icon">
          <text class="device-icon-text">BLE</text>
        </view>
        <text class="device-name">{{ deviceName }}</text>
        <text class="device-mac">{{ deviceId }}</text>
        <view class="device-status-badge">
          <view :class="['status-dot', { connected: isConnected }]"></view>
          <text :class="['status-label', { connected: isConnected }]">
            {{ isConnected ? '在线' : '离线' }}
          </text>
        </view>
      </view>
    </view>

    <view class="content-area">
      <view class="control-card">
        <view class="card-item" @tap="toggleMode">
          <view class="card-item-left">
            <text class="card-item-title">工作模式</text>
            <text class="card-item-desc">{{ modes.find(m => m.value === currentMode)?.label || '未知' }}</text>
          </view>
          <view class="mode-switch">
            <view :class="['switch-option', { active: currentMode === 0 }]" @tap.stop="setMode(0)">
              <text>ESC</text>
            </view>
            <view :class="['switch-option', { active: currentMode === 1 }]" @tap.stop="setMode(1)">
              <text>PWM</text>
            </view>
          </view>
        </view>

        <view class="card-divider"></view>

        <view class="card-item">
          <view class="card-item-row">
            <text class="card-item-title">调节速度</text>
            <text class="card-item-value">{{ targetProgress }}%</text>
          </view>
          <view class="custom-slider-container">
            <view class="capsule-slider" @touchstart="onSliderTouchStart" @touchmove.stop.prevent="onSliderTouchMove" @touchend="onSliderTouchEnd">
              <view class="capsule-slider-track">
                <view class="capsule-slider-fill" :style="{ width: targetProgress + '%' }"></view>
              </view>
              <view class="capsule-slider-thumb-area">
                <view class="capsule-slider-thumb" :style="{ left: targetProgress + '%' }"></view>
              </view>
            </view>
          </view>
        </view>
      </view>

      <view class="data-row">
        <view class="data-card">
          <text class="data-label">电压</text>
          <text class="data-value">{{ status.v.toFixed(2) }}</text>
          <text class="data-unit">V</text>
        </view>
        <view class="data-card">
          <text class="data-label">电流</text>
          <text class="data-value">{{ status.i.toFixed(2) }}</text>
          <text class="data-unit">A</text>
        </view>
        <view class="data-card">
          <text class="data-label">功率</text>
          <text class="data-value">{{ (status.v * status.i).toFixed(1) }}</text>
          <text class="data-unit">W</text>
        </view>
      </view>

      <view class="action-row">
        <view class="action-btn primary-btn" @tap="sendCommand">
          <text class="action-btn-text">确认</text>
        </view>
        <view class="action-btn secondary-btn" @tap="refreshStatus">
          <text class="action-btn-text">刷新</text>
        </view>
        <view class="action-btn danger-btn" @tap="disconnect">
          <text class="action-btn-text">断开</text>
        </view>
      </view>

      <view class="log-card">
        <text class="log-title">通信日志</text>
        <view class="log-list" v-if="logs.length > 0">
          <view
            v-for="(log, index) in logs"
            :key="index"
            class="log-item"
          >
            <text class="log-time">{{ log.time }}</text>
            <text class="log-msg">{{ log.message }}</text>
          </view>
        </view>
        <view class="empty-log" v-else>
          <text class="empty-log-text">暂无通信记录</text>
        </view>
      </view>
    </view>
  </view>
</template>

<script>
import { ref, onMounted, onUnmounted } from 'vue'
import Taro, { useRouter } from '@tarojs/taro'
import { bleManager } from '../../utils/ble'
import './index.css'

export default {
  setup() {
    const router = useRouter()
    const deviceId = ref('')
    const deviceName = ref('')
    const isConnected = ref(false)
    const status = ref({
      v: 0,
      i: 0,
      mode: 0,
      progress: 0
    })
    const currentMode = ref(0)
    const targetProgress = ref(50)
    const logs = ref([])
    let hasLocalChanges = false

    const modes = [
      { label: 'ESC电调', value: 0 },
      { label: 'PWM模式', value: 1 }
    ]

    const addLog = (message) => {
      const now = new Date()
      const time = `${now.getHours().toString().padStart(2, '0')}:${now.getMinutes().toString().padStart(2, '0')}:${now.getSeconds().toString().padStart(2, '0')}`
      logs.value.unshift({ time, message })
      if (logs.value.length > 50) {
        logs.value.pop()
      }
    }

    const setMode = (mode) => {
      currentMode.value = mode
      hasLocalChanges = true
      addLog(`设置模式: ${modes.find(m => m.value === mode)?.label}`)
    }

    const toggleMode = () => {
      currentMode.value = currentMode.value === 0 ? 1 : 0
      hasLocalChanges = true
      addLog(`切换模式: ${modes.find(m => m.value === currentMode.value)?.label}`)
    }

    const onProgressChange = (e) => {
      targetProgress.value = e.detail.value
      hasLocalChanges = true
    }

    let sliderRect = null

    const getSliderRect = async () => {
      return new Promise((resolve) => {
        Taro.createSelectorQuery()
          .select('.capsule-slider-thumb-area')
          .boundingClientRect((rect) => {
            sliderRect = rect
            resolve(rect)
          })
          .exec()
      })
    }

    const calcProgressFromTouch = (touchX) => {
      if (!sliderRect) return targetProgress.value
      const ratio = (touchX - sliderRect.left) / sliderRect.width
      const clamped = Math.min(1, Math.max(0, ratio))
      return Math.round(clamped * 100)
    }

    const onSliderTouchStart = async (e) => {
      await getSliderRect()
      if (sliderRect && e.touches[0]) {
        const newVal = calcProgressFromTouch(e.touches[0].clientX)
        if (newVal !== targetProgress.value) {
          targetProgress.value = newVal
          hasLocalChanges = true
        }
      }
    }

    const onSliderTouchMove = (e) => {
      if (sliderRect && e.touches[0]) {
        const newVal = calcProgressFromTouch(e.touches[0].clientX)
        if (newVal !== targetProgress.value) {
          targetProgress.value = newVal
          hasLocalChanges = true
        }
      }
    }

    const onSliderTouchEnd = () => {
      sliderRect = null
    }

    const sendCommand = async () => {
      const data = `M=${currentMode.value}&P=${targetProgress.value}`
      addLog(`发送: ${data}`)

      const success = await bleManager.sendData(data)
      if (success) {
        hasLocalChanges = false
        Taro.showToast({ title: '已确认', icon: 'success' })
        addLog('发送成功')
      } else {
        Taro.showToast({ title: '发送失败', icon: 'none' })
        addLog('发送失败')
      }
    }

    let pollTimer = null

    const startPolling = () => {
      stopPolling()
      pollTimer = setInterval(async () => {
        if (!bleManager.isConnected()) return
        try {
          await bleManager.readStatus()
        } catch {}
      }, 5000)
    }

    const stopPolling = () => {
      if (pollTimer) {
        clearInterval(pollTimer)
        pollTimer = null
      }
    }

    const requestStatusWithRetry = async (retries = 3) => {
      for (let i = 0; i < retries; i++) {
        addLog(`查询状态 (${i + 1}/${retries})...`)

        try {
          await bleManager.readStatus()
        } catch {}

        const success = await bleManager.sendHeartbeat()
        if (success) {
          addLog('心跳已发送')
        } else {
          addLog('发送失败，重试...')
          await new Promise(resolve => setTimeout(resolve, 500))
          continue
        }

        const beforeV = status.value.v
        const beforeI = status.value.i
        await new Promise(resolve => setTimeout(resolve, 1500))

        if (status.value.v !== beforeV || status.value.i !== beforeI) {
          addLog(`收到设备响应: V=${status.value.v}V, I=${status.value.i}A`)
          return
        }

        if (i < retries - 1) {
          addLog('未收到响应，重试...')
        }
      }
      addLog('多次查询未收到设备响应')
    }

    const refreshStatus = async () => {
      await requestStatusWithRetry(3)
    }

    const disconnect = async () => {
      Taro.showModal({
        title: '确认断开',
        content: '确定要断开设备连接吗？',
        success: async (res) => {
          if (res.confirm) {
            await bleManager.disconnect(deviceId.value)
            isConnected.value = false
            addLog('设备已断开')
            Taro.showToast({ title: '已断开', icon: 'success' })
            setTimeout(() => {
              Taro.navigateBack()
            }, 1500)
          }
        }
      })
    }

    const handleStatusUpdate = (newStatus) => {
      status.value.v = newStatus.v
      status.value.i = newStatus.i
      if (!hasLocalChanges) {
        status.value.mode = newStatus.mode
        status.value.progress = newStatus.progress
        currentMode.value = newStatus.mode
        targetProgress.value = newStatus.progress
      }
      addLog(`收到: V=${newStatus.v}V, I=${newStatus.i}A, 模式=${modes.find(m => m.value === newStatus.mode)?.label || newStatus.mode}, 进度=${newStatus.progress}%`)
    }

    onMounted(async () => {
      deviceId.value = router.params.deviceId || ''
      deviceName.value = decodeURIComponent(router.params.name || '未知设备')
      isConnected.value = bleManager.isConnected()

      bleManager.onStatusChange(handleStatusUpdate)

      addLog('页面已打开')
      if (isConnected.value) {
        addLog(`已连接: ${deviceId.value}`)

        addLog('刷新BLE订阅...')
        const subOk = await bleManager.refreshNotifySubscription()
        addLog(subOk ? 'BLE订阅已刷新' : 'BLE订阅刷新失败')

        setTimeout(() => {
          requestStatusWithRetry(3)
        }, 500)

        startPolling()
      } else {
        addLog('设备未连接')
      }
    })

    onUnmounted(() => {
      bleManager.onStatusChange(null)
      stopPolling()
    })

    return {
      deviceId,
      deviceName,
      isConnected,
      status,
      currentMode,
      targetProgress,
      logs,
      modes,
      setMode,
      toggleMode,
      onProgressChange,
      onSliderTouchStart,
      onSliderTouchMove,
      onSliderTouchEnd,
      sendCommand,
      refreshStatus,
      disconnect
    }
  }
}
</script>
