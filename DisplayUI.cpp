#include "DisplayUI.h"

static float normalizeForDisplay(float value, float epsilon) {
    return (value > -epsilon && value < epsilon) ? 0.0f : value;
}

// ======================== 风车位图数据 (32x32, 逐行式, MSB-first) ========================
static constexpr int FAN_ICON_W = 32;
static constexpr int FAN_ICON_H = 32;
const uint8_t fengcheData[] = {
    0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x7F, 0xC0, 0x0, 0x0, 0x7F, 0x80, 0x6, 0x2, 0x7F, 0x0, 0x7, 0x6, 0x7E, 0x0, 0x7, 0x8E, 0x7C, 0x0, 0x7, 0xCE, 0x78, 0x0, 0x7, 0xCE, 0x70, 0x0, 0x7, 0xE6, 0x66, 0x0, 0x7, 0xF0, 0xF, 0x0, 0x7, 0xF9, 0x9F, 0x80, 0x0, 0x1, 0xC0, 0x0, 0x0, 0x3, 0xC0, 0x0, 0x1, 0xF9, 0x9F, 0xE0, 0x0, 0xF8, 0xF, 0xE0, 0x0, 0x76, 0x67, 0xE0, 0x0, 0x2E, 0x73, 0xE0, 0x0, 0x1E, 0x7B, 0xE0, 0x0, 0x3E, 0x71, 0xE0, 0x0, 0x7E, 0x60, 0xE0, 0x0, 0xFE, 0x40, 0x60, 0x1, 0xFE, 0x0, 0x0, 0x1, 0xFE, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0
};

// MSB-first → LSB-first 位反转查找表 (drawXBM 需要 LSB-first)
static const uint8_t bitReverseLUT[256] = {
    0x00,0x80,0x40,0xC0,0x20,0xA0,0x60,0xE0,0x10,0x90,0x50,0xD0,0x30,0xB0,0x70,0xF0,
    0x08,0x88,0x48,0xC8,0x28,0xA8,0x68,0xE8,0x18,0x98,0x58,0xD8,0x38,0xB8,0x78,0xF8,
    0x04,0x84,0x44,0xC4,0x24,0xA4,0x64,0xE4,0x14,0x94,0x54,0xD4,0x34,0xB4,0x74,0xF4,
    0x0C,0x8C,0x4C,0xCC,0x2C,0xAC,0x6C,0xEC,0x1C,0x9C,0x5C,0xDC,0x3C,0xBC,0x7C,0xFC,
    0x02,0x82,0x42,0xC2,0x22,0xA2,0x62,0xE2,0x12,0x92,0x52,0xD2,0x32,0xB2,0x72,0xF2,
    0x0A,0x8A,0x4A,0xCA,0x2A,0xAA,0x6A,0xEA,0x1A,0x9A,0x5A,0xDA,0x3A,0xBA,0x7A,0xFA,
    0x06,0x86,0x46,0xC6,0x26,0xA6,0x66,0xE6,0x16,0x96,0x56,0xD6,0x36,0xB6,0x76,0xF6,
    0x0E,0x8E,0x4E,0xCE,0x2E,0xAE,0x6E,0xEE,0x1E,0x9E,0x5E,0xDE,0x3E,0xBE,0x7E,0xFE,
    0x01,0x81,0x41,0xC1,0x21,0xA1,0x61,0xE1,0x11,0x91,0x51,0xD1,0x31,0xB1,0x71,0xF1,
    0x09,0x89,0x49,0xC9,0x29,0xA9,0x69,0xE9,0x19,0x99,0x59,0xD9,0x39,0xB9,0x79,0xF9,
    0x05,0x85,0x45,0xC5,0x25,0xA5,0x65,0xE5,0x15,0x95,0x55,0xD5,0x35,0xB5,0x75,0xF5,
    0x0D,0x8D,0x4D,0xCD,0x2D,0xAD,0x6D,0xED,0x1D,0x9D,0x5D,0xDD,0x3D,0xBD,0x7D,0xFD,
    0x03,0x83,0x43,0xC3,0x23,0xA3,0x63,0xE3,0x13,0x93,0x53,0xD3,0x33,0xB3,0x73,0xF3,
    0x0B,0x8B,0x4B,0xCB,0x2B,0xAB,0x6B,0xEB,0x1B,0x9B,0x5B,0xDB,0x3B,0xBB,0x7B,0xFB,
    0x07,0x87,0x47,0xC7,0x27,0xA7,0x67,0xE7,0x17,0x97,0x57,0xD7,0x37,0xB7,0x77,0xF7,
    0x0F,0x8F,0x4F,0xCF,0x2F,0xAF,0x6F,0xEF,0x1F,0x9F,0x5F,0xDF,0x3F,0xBF,0x7F,0xFF
};

// 将 MSB-first 位图转为 LSB-first
static void convertMSBtoLSB(const uint8_t* src, uint8_t* dst, int len) {
    for (int i = 0; i < len; i++) {
        dst[i] = bitReverseLUT[src[i]];
    }
}

// 旋转32x32 XBM位图 (任意角度顺时针, 最近邻采样)
static void rotateXBM32(const uint8_t* src, uint8_t* dst, float angleDeg) {
    memset(dst, 0, 128);
    float rad = -angleDeg * 3.14159265f / 180.0f; // 顺时针旋转用负角
    float cosA = cosf(rad);
    float sinA = sinf(rad);
    float cx = 15.5f, cy = 15.5f; // 32x32 中心
    for (int dy = 0; dy < 32; dy++) {
        for (int dx = 0; dx < 32; dx++) {
            // 逆映射到源坐标
            float sx = cx + (dx - cx) * cosA + (dy - cy) * sinA;
            float sy = cy - (dx - cx) * sinA + (dy - cy) * cosA;
            int isx = (int)(sx + 0.5f);
            int isy = (int)(sy + 0.5f);
            if (isx < 0 || isx >= 32 || isy < 0 || isy >= 32) continue;
            int srcBit = (src[(isy * 4) + (isx / 8)] >> (isx % 8)) & 1;
            if (srcBit) {
                dst[(dy * 4) + (dx / 8)] |= (1 << (dx % 8));
            }
        }
    }
}

// ======================== 核心逻辑 ========================

void DisplayUI::drawBootScreen(const char* version) {
    const uint32_t BOOT_DURATION_MS = 3000;
    const uint32_t startTime = millis();

    while (millis() - startTime < BOOT_DURATION_MS) {
        u8g2->clearBuffer();

        int elapsed = millis() - startTime;
        int percent = (elapsed * 100) / BOOT_DURATION_MS;
        if (percent > 100) percent = 100;

        u8g2->setFont(u8g2_font_profont11_tr);

        // 标题
        u8g2->setCursor(2, 13);
        u8g2->print("FENG PWM Controller");

        // 副标题
        u8g2->setCursor(2, 28);
        u8g2->print("Initializing...");

        // 字符串进度条: [##########] 100%
        const int barLen = 16;
        int filled = (percent * barLen + 50) / 100;
        if (filled > barLen) filled = barLen;
        if (filled < 0) filled = 0;
        char barBuf[28];
        int pos = 0;
        barBuf[pos++] = '[';
        for (int i = 0; i < barLen; i++) {
            barBuf[pos++] = (i < filled) ? '#' : '-';
        }
        barBuf[pos++] = ']';
        barBuf[pos] = ' ';
        pos++;
        pos += snprintf(barBuf + pos, sizeof(barBuf) - pos, "%3d%%", percent);
        barBuf[pos] = '\0';

        u8g2->setCursor(2, 43);
        u8g2->print(barBuf);

        // 版本号右下角
        int vw = u8g2->getStrWidth(version);
        u8g2->setCursor(126 - vw, 58);
        u8g2->print(version);

        // 100% 时显示 "Done." 并短暂停留
        if (percent >= 100) {
            u8g2->setCursor(2, 58);
            u8g2->print("Done.");
        }

        u8g2->sendBuffer();
        delay(30); // ~33fps
    }
}

void DisplayUI::init(U8G2 *u8g2_ptr) {
    u8g2 = u8g2_ptr;
    u8g2->enableUTF8Print();  // 启用UTF8以支持中文显示
}

void DisplayUI::processInput(int encoderDelta, bool isLongPress, bool isShortPress) {
    unsigned long now = millis();

    // 处理长按逻辑 (长按循环切换屏幕)
    if (isLongPress) {
        if (currentScreen == SCREEN_FAN) {
            currentScreen = SCREEN_MENU;
        } else if (currentScreen == SCREEN_MENU) {
            currentScreen = SCREEN_INFO;
        } else {
            currentScreen = SCREEN_FAN;
        }
        isEditing = false; // 切换屏幕时强制退出编辑模式
        return; // 长按处理完毕后直接返回
    }

    // 处理短按逻辑 (MiaoUI 风格: 单击进入编辑/单击退出编辑)
    if (isShortPress) {
        // 双击紧急停止：任意界面双击立刻归零
        if (now - lastShortPressTime < DOUBLE_CLICK_MS) {
            progress = 0;
            lastShortPressTime = 0; // 防止三击再触发
            return;
        }
        lastShortPressTime = now;

        if (currentScreen == SCREEN_MENU) {
            if (!isEditing) {
                isEditing = true; // 进入编辑模式
                // 记录当前的开关状态作为临时状态，供旋转时预览
                tempBtState = btConnected;
                tempWifiState = wifiConnected;
                tempModeEsc = modeEsc;
                tempFlipEnabled = flipEnabled;
                tempSleepEnabled = sleepEnabled;
                tempBrightness = brightness;
            } else {
                isEditing = false; // 退出编辑模式
                lastMenuSwitchTime = now;
                // 检查在编辑模式下是否改变了开关状态，如果改变了则触发主程序的 Toggle
                if (currentFocus == MENU_MODE && tempModeEsc != modeEsc) {
                    flagToggleMode = true;
                }
                if (currentFocus == MENU_BT && tempBtState != btConnected) {
                    flagToggleBt = true;
                }
                if (currentFocus == MENU_WIFI && tempWifiState != wifiConnected) {
                    flagToggleWifi = true;
                }
                if (currentFocus == MENU_FLIP && tempFlipEnabled != flipEnabled) {
                    flagToggleFlip = true;
                }
                if (currentFocus == MENU_SLEEP && tempSleepEnabled != sleepEnabled) {
                    flagToggleSleep = true;
                }
                if (currentFocus == MENU_BRIGHTNESS && tempBrightness != brightness) {
                    brightness = tempBrightness;
                    flagBrightnessChange = true;
                }
            }
        }
    }
    // 处理旋钮逻辑
    else if (encoderDelta != 0) {
        if (currentScreen == SCREEN_FAN) {
            // 风扇主面板：旋转直接调整转速
            if (now - lastSpeedAdjustTime < SPEED_ADJUST_THROTTLE_MS) {
                return;
            }

            // 根据旋转速度决定步进值
            unsigned long tickInterval = now - lastEncoderTickTime;
            lastEncoderTickTime = now;
            
            // 连续快速转动计数
            if (tickInterval < FAST_TICK_MS) {
                encoderAccel++;
            } else if (tickInterval > IDLE_TICK_MS) {
                encoderAccel = 0;
            }
            
            int step;
            if (encoderAccel >= 7) {
                // 连续快速转动7次后，步进5
                step = 5;
            } else {
                // 慢速转动或刚开始快速转动，步进1
                step = 1;
            }
            if (encoderDelta < 0) step = -step;

            progress += step;
            if (progress > 100) progress = 100;
            if (progress < 0) progress = 0;
            lastSpeedAdjustTime = now;
        } else if (currentScreen == SCREEN_MENU) {
            if (!isEditing) {
                // 在浏览模式下，增加节流：固定时间窗内最多切换一级，避免机械抖动导致跳多级
                if (now - lastMenuSwitchTime < MENU_SWITCH_THROTTLE_MS) {
                    return;
                }

                // 在浏览模式下，左右旋转切换菜单焦点（单次仅切一格）
                int newFocus = (int)currentFocus;
                if (encoderDelta > 0) {
                    newFocus++;
                } else {
                    newFocus--;
                }
                
                if (newFocus > TOTAL_MENU_ITEMS - 1) newFocus = TOTAL_MENU_ITEMS - 1;
                if (newFocus < 0) newFocus = 0;
                currentFocus = (FocusItem)newFocus;

                if (newFocus < firstVisibleIndex) {
                    firstVisibleIndex = newFocus;
                } else if (newFocus >= firstVisibleIndex + VISIBLE_MENU_ROWS) {
                    firstVisibleIndex = newFocus - VISIBLE_MENU_ROWS + 1;
                }

                lastMenuSwitchTime = now;
            } else {
                // 在编辑模式下，左右旋转调整当前选中的数值
                if (currentFocus == MENU_SPEED) {
                    if (now - lastSpeedAdjustTime < SPEED_ADJUST_THROTTLE_MS) {
                        return;
                    }
                    
                    // 根据旋转速度决定步进值
                    unsigned long tickInterval = now - lastEncoderTickTime;
                    lastEncoderTickTime = now;
                    
                    // 连续快速转动计数
                    if (tickInterval < FAST_TICK_MS) {
                        encoderAccel++;
                    } else if (tickInterval > IDLE_TICK_MS) {
                        encoderAccel = 0;
                    }
                    
                    int step;
                    if (encoderAccel >= 7) {
                        // 连续快速转动7次后，步进5
                        step = 5;
                    } else {
                        // 慢速转动或刚开始快速转动，步进1
                        step = 1;
                    }
                    if (encoderDelta < 0) step = -step;
                    
                    progress += step;
                    if (progress > 100) progress = 100;
                    if (progress < 0) progress = 0;
                    lastSpeedAdjustTime = now;
                } else if (currentFocus == MENU_MODE) {
                    if (now - lastModeToggleTime < MODE_TOGGLE_THROTTLE_MS) {
                        return;
                    }
                    tempModeEsc = !tempModeEsc;
                    lastModeToggleTime = now;
                } else if (currentFocus == MENU_BT) {
                    // 任意方向旋转均切换临时预览状态
                    tempBtState = !tempBtState;
                } else if (currentFocus == MENU_WIFI) {
                    // 任意方向旋转均切换临时预览状态
                    tempWifiState = !tempWifiState;
                } else if (currentFocus == MENU_FLIP) {
                    tempFlipEnabled = !tempFlipEnabled;
                } else if (currentFocus == MENU_SLEEP) {
                    tempSleepEnabled = !tempSleepEnabled;
                } else if (currentFocus == MENU_BRIGHTNESS) {
                    if (now - lastSpeedAdjustTime < SPEED_ADJUST_THROTTLE_MS) {
                        return;
                    }
                    int step = (encoderDelta > 0) ? 10 : -10;
                    tempBrightness += step;
                    if (tempBrightness > 255) tempBrightness = 255;
                    if (tempBrightness < 0) tempBrightness = 0;
                    lastSpeedAdjustTime = now;
                }
            }
        }
    }
}

void DisplayUI::render() {
    u8g2->clearBuffer();
    
    if (currentScreen == SCREEN_FAN) {
        drawScreen0();
    } else if (currentScreen == SCREEN_MENU) {
        drawScreen1();
    } else {
        drawScreen2();
    }
    
    u8g2->sendBuffer();
}

// ======================== 风扇控制主面板 (第0屏) ========================
void DisplayUI::drawScreen0() {
    unsigned long now = millis();

    // 风扇图标旋转动画
    // 二次曲线映射: 低速可见(3fps), 高速流畅(20fps), 更符合真实风扇手感
    // fps = 3 + 17 * (speed/100)^2
    //   1% → 3fps, 25% → 4fps, 50% → 7fps, 75% → 13fps, 100% → 20fps
    float targetFps = 0;
    if (displayProgress > 0) {
        float speedRatio = displayProgress / 100.0f;
        targetFps = 3.0f + 17.0f * speedRatio * speedRatio;
    }

    // 平滑加减速: 指数平滑让风扇逐渐加速/减速，而非瞬间跳变
    float speedDiff = targetFps - fanAnimSpeed;
    fanAnimSpeed += speedDiff * 0.12f;
    if (targetFps == 0 && fanAnimSpeed < 0.5f) fanAnimSpeed = 0;

    unsigned long fanInterval = (fanAnimSpeed > 0.5f) ? (unsigned long)(1000.0f / fanAnimSpeed) : 0xFFFFFFFFUL;
    if (now - lastFanAnimTime >= fanInterval) {
        fanAnimPhase = (fanAnimPhase + 1) % 16;
        lastFanAnimTime = now;
    }

    // 进度条立即生效
    displayProgress = progress;

    // ===== 右侧：风扇图标 (32x32) =====
    const int iconX = 128 - 32 - 6;  // 右侧 padding=6, 与左侧对称
    const int iconY = 0;
    float angle = fanAnimPhase * 22.5f; // 16阶段, 每步22.5°
    uint8_t lsbBuf[128];
    convertMSBtoLSB(fengcheData, lsbBuf, 128);
    if (fanAnimPhase == 0) {
        u8g2->drawXBM(iconX, iconY, FAN_ICON_W, FAN_ICON_H, lsbBuf);
    } else {
        uint8_t rotatedBuf[128];
        rotateXBM32(lsbBuf, rotatedBuf, angle);
        u8g2->drawXBM(iconX, iconY, FAN_ICON_W, FAN_ICON_H, rotatedBuf);
    }

    // ===== 左侧：电压/电流/功率 信息区 =====
    // 统一5位小数，电流低于0.001A取绝对值避免负号跳变
    float dv = normalizeForDisplay(voltage, 0.000005f);
    float dc = normalizeForDisplay(current, 0.000005f);
    if (fabs(dc) < 0.001f) dc = fabs(dc);
    float pwr = normalizeForDisplay(dv * dc, 0.000005f);

    const int padX = 6;    // 左侧 padding
    const int tagW = 12;   // 标签背景宽度
    const int tagH = 12;   // 标签背景高度
    const int gapX = 3;    // 标签与数值间距

    // 第1行: U (电压)
    int row1Y = 2;
    u8g2->setFont(u8g2_font_profont11_tr);
    u8g2->drawBox(padX, row1Y, tagW, tagH);
    u8g2->setDrawColor(0);
    u8g2->setCursor(padX + 2, row1Y + 10);
    u8g2->print("U");
    u8g2->setDrawColor(1);
    u8g2->setCursor(padX + tagW + gapX, row1Y + 10);
    char uStr[16];
    snprintf(uStr, sizeof(uStr), "%.5fV", dv);
    u8g2->print(uStr);

    // 第2行: I (电流)
    int row2Y = 18;
    u8g2->drawBox(padX, row2Y, tagW, tagH);
    u8g2->setDrawColor(0);
    u8g2->setCursor(padX + 3, row2Y + 10);
    u8g2->print("I");
    u8g2->setDrawColor(1);
    u8g2->setCursor(padX + tagW + gapX, row2Y + 10);
    char iStr[16];
    snprintf(iStr, sizeof(iStr), "%.5fA", dc);
    u8g2->print(iStr);

    // 第3行: P (功率)
    int row3Y = 34;
    u8g2->drawBox(padX, row3Y, tagW, tagH);
    u8g2->setDrawColor(0);
    u8g2->setCursor(padX + 2, row3Y + 10);
    u8g2->print("P");
    u8g2->setDrawColor(1);
    u8g2->setCursor(padX + tagW + gapX, row3Y + 10);
    char pStr[16];
    snprintf(pStr, sizeof(pStr), "%.5fW", pwr);
    u8g2->print(pStr);

    // ===== 最底部：极客风格字符进度条 =====
    // [##########] 100%  进度条 + 百分比同行
    u8g2->setFont(u8g2_font_profont11_tr);
    const int barLen = 12;
    int filled = (displayProgress * barLen + 50) / 100;
    if (filled > barLen) filled = barLen;
    if (filled < 0) filled = 0;

    // 拼接: [##########] 100%
    char fullBar[24];
    int pos = 0;
    fullBar[pos++] = '[';
    for (int i = 0; i < barLen; i++) {
        fullBar[pos++] = (i < filled) ? '#' : '-';
    }
    fullBar[pos++] = ']';
    fullBar[pos] = ' ';
    pos++;
    // 追加百分比
    pos += snprintf(fullBar + pos, sizeof(fullBar) - pos, "%3d%%", displayProgress);

    u8g2->setCursor(padX, 60);
    u8g2->print(fullBar);
}

void DisplayUI::drawScreen1() {
    // ================== 头部状态栏 (电量/功率) ==================
    // 使用带边框的像素字体，或者数字更清晰的字体
    u8g2->setFont(u8g2_font_profont11_tr);
    u8g2->setDrawColor(1);
    
    // 左上角: 电池电压
    float displayBattVoltage = normalizeForDisplay(battVoltage, 0.05f); // 1位小数显示
    u8g2->setCursor(2, 9);
    u8g2->print("BAT:");
    u8g2->print(displayBattVoltage, 1);
    u8g2->print("V");
    
    // 右上角: 实时功率 (V * A)
    float displayVoltage = normalizeForDisplay(voltage, 0.0005f);
    float displayCurrent = normalizeForDisplay(current, 0.0005f);
    float pwr = normalizeForDisplay(displayVoltage * displayCurrent, 0.05f); // 1位小数显示
    char pwrStr[16];
    snprintf(pwrStr, sizeof(pwrStr), "PWR:%.1fW", pwr);
    int pwrWidth = u8g2->getStrWidth(pwrStr);
    u8g2->setCursor(126 - pwrWidth, 9);
    u8g2->print(pwrStr);

    // 头部下划线 (分隔线)
    u8g2->drawLine(0, 11, 128, 11);

    // ================== 菜单列表 ==================
    // MiaoUI 风格：垂直列表，选中的项有圆角反色框
    const char* labels[] = {"Speed", "Mode", "Bluetooth", "Hotspot", "Flip", "AutoSleep", "Bright"};
    
    int focusIndex = (int)currentFocus;
    if (focusIndex < firstVisibleIndex) {
        firstVisibleIndex = focusIndex;
    } else if (focusIndex >= firstVisibleIndex + VISIBLE_MENU_ROWS) {
        firstVisibleIndex = focusIndex - VISIBLE_MENU_ROWS + 1;
    }

    if (firstVisibleIndex < 0) firstVisibleIndex = 0;
    if (firstVisibleIndex > TOTAL_MENU_ITEMS - VISIBLE_MENU_ROWS) {
        firstVisibleIndex = TOTAL_MENU_ITEMS - VISIBLE_MENU_ROWS;
    }
    if (firstVisibleIndex < 0) firstVisibleIndex = 0;

    for (int row = 0; row < VISIBLE_MENU_ROWS; row++) {
        int i = firstVisibleIndex + row;
        if (i >= TOTAL_MENU_ITEMS) break;

        int y = 14 + row * 16; // 使用16像素行高，滚动显示
        
        // 统一字体
        u8g2->setFont(u8g2_font_profont11_tr);
        u8g2->setCursor(6, y + 12);
        u8g2->print(labels[i]);

        // 在 Speed 文本后方显示分段式进度条 (Geek 仪表风格)
        if (i == MENU_SPEED) {
            const int barX = 44;
            const int barY = y + 5;
            const int segW = 4;
            const int segH = 6;
            const int gap = 1;
            
            // 四舍五入计算点亮的格数 (0 - 10)
            int filledSegments = (progress + 5) / 10; 
            if (filledSegments > 10) filledSegments = 10;
            
            for (int j = 0; j < 10; j++) {
                int segX = barX + j * (segW + gap);
                if (j < filledSegments) {
                    u8g2->drawBox(segX, barY, segW, segH);   // 实心块代表已达到的进度
                } else {
                    u8g2->drawFrame(segX, barY, segW, segH); // 空心框代表未达到的进度
                }
            }
        }
        
        // 准备右侧的数值字符串
        char valStr[16];
        if (i == 0) {
            snprintf(valStr, sizeof(valStr), "%d", progress);
        } else if (i == 1) {
            bool esc = (isEditing && currentFocus == i) ? tempModeEsc : modeEsc;
            snprintf(valStr, sizeof(valStr), esc ? "ESC" : "FAN");
        } else if (i == 2) {
            bool state = (isEditing && currentFocus == i) ? tempBtState : btConnected;
            snprintf(valStr, sizeof(valStr), state ? "ON" : "OFF");
        } else if (i == 3) {
            bool state = (isEditing && currentFocus == i) ? tempWifiState : wifiConnected;
            snprintf(valStr, sizeof(valStr), state ? "ON" : "OFF");
        } else if (i == 4) {
            bool state = (isEditing && currentFocus == i) ? tempFlipEnabled : flipEnabled;
            snprintf(valStr, sizeof(valStr), state ? "180" : "0");
        } else if (i == 5) {
            bool state = (isEditing && currentFocus == i) ? tempSleepEnabled : sleepEnabled;
            snprintf(valStr, sizeof(valStr), state ? "ON" : "OFF");
        } else if (i == 6) {
            int b = (isEditing && currentFocus == i) ? tempBrightness : brightness;
            snprintf(valStr, sizeof(valStr), "%d", b);
        }
        
        // 统一字体
        u8g2->setFont(u8g2_font_profont11_tr);
        
        // 测量数值字符串宽度以实现右对齐
        int strWidth = u8g2->getStrWidth(valStr);
        int valX = 122 - strWidth;
        
        if (i == 0) {
             u8g2->setCursor(valX - 8, y + 12);
             u8g2->print(valStr);
             // 追加百分号
             u8g2->setFont(u8g2_font_profont11_tr);
             u8g2->setCursor(124 - u8g2->getStrWidth("%"), y + 12);
             u8g2->print("%");
             strWidth = u8g2->getStrWidth(valStr) + 10; // 重新计算反色框宽度
        } else {
             u8g2->setCursor(valX, y + 12);
             u8g2->print(valStr);
        }
        
        // ====== 绘制选中/编辑的高亮框 ======
        if (focusIndex == i) {
            u8g2->setDrawColor(2); // 设置为 XOR 异或模式，重叠区域会自动反色 (黑变白，白变黑)
            
            if (isEditing) {
                // 编辑模式：仅高亮右侧的数值区域，形成“选中值”的视觉反馈
                if (i == 0) {
                     u8g2->drawRBox(valX - 14, y, strWidth + 8, 15, 2);
                } else {
                     u8g2->drawRBox(valX - 4, y, strWidth + 8, 15, 2);
                }
            } else {
                // 浏览模式：高亮整行菜单
                u8g2->drawRBox(0, y, 128, 15, 2);
            }
            
            u8g2->setDrawColor(1); // 恢复正常绘制颜色 (避免影响下一行)
        }
    }

    // 滚动提示：有上/下隐藏项时显示小三角
    if (firstVisibleIndex > 0) {
        u8g2->drawTriangle(124, 14, 127, 14, 125, 12);
    }
    if (firstVisibleIndex + VISIBLE_MENU_ROWS < TOTAL_MENU_ITEMS) {
        u8g2->drawTriangle(124, 61, 127, 61, 125, 63);
    }
}

// ======================== 辅助虚线绘制函数 ========================
static void drawDashedHLine(U8G2 *u8g2, int x, int y, int w) {
    for (int i = 0; i < w; i += 4) {
        u8g2->drawHLine(x + i, y, 2);
    }
}

static void drawDashedVLine(U8G2 *u8g2, int x, int y, int h) {
    for (int i = 0; i < h; i += 4) {
        u8g2->drawVLine(x, y + i, 2);
    }
}

// ======================== 绘制第二屏 ========================
void DisplayUI::drawScreen2() {
    // 第二屏改为单列四行列表：U/I/P/T
    // 每行左侧字母为白字黑底方块，右侧显示对应数值
    const int rowY[4] = {12, 27, 42, 57};
    const char labels[4] = {'U', 'I', 'P', 'T'};

    float displayVoltage = normalizeForDisplay(voltage, 0.0005f);          // 3位小数
    float displayCurrent = normalizeForDisplay(current, 0.0005f);          // 3位小数
    float displayPower = normalizeForDisplay(displayVoltage * displayCurrent, 0.005f); // 2位小数

    char valueStr[4][18];
    snprintf(valueStr[0], sizeof(valueStr[0]), "%.3f V", displayVoltage);
    snprintf(valueStr[1], sizeof(valueStr[1]), "%.3f A", displayCurrent);
    snprintf(valueStr[2], sizeof(valueStr[2]), "%.2f W", displayPower);
    snprintf(valueStr[3], sizeof(valueStr[3]), "-");

    u8g2->setFont(u8g2_font_profont11_tr);

    for (int i = 0; i < 4; i++) {
        int y = rowY[i];

        // 左侧字母方块
        u8g2->drawBox(4, y - 9, 12, 12);
        u8g2->setDrawColor(0);
        u8g2->setCursor(8, y);
        u8g2->print(labels[i]);
        u8g2->setDrawColor(1);

        // 右侧数值
        u8g2->setCursor(22, y);
        u8g2->print(valueStr[i]);
    }
}

// display ui
