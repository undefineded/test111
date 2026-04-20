#include "DisplayUI.h"

// ======================== 核心逻辑 ========================

void DisplayUI::init(U8G2 *u8g2_ptr) {
    u8g2 = u8g2_ptr;
    u8g2->enableUTF8Print();  // 启用UTF8以支持中文显示
}

void DisplayUI::processInput(int encoderDelta, bool isLongPress, bool isShortPress) {
    unsigned long now = millis();

    // 处理短按逻辑 (MiaoUI 风格: 单击进入编辑/单击退出编辑)
    if (isShortPress) {
        if (!isEditing) {
            isEditing = true; // 进入编辑模式
            // 记录当前的开关状态作为临时状态，供旋转时预览
            tempBtState = btConnected;
            tempWifiState = wifiConnected;
            tempModeEsc = modeEsc;
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
        }
    }
    // 处理旋钮逻辑
    else if (encoderDelta != 0) {
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
                progress += encoderDelta * 2; // 适当加快步进速度 (2%)
                if (progress > 100) progress = 100;
                if (progress < 0) progress = 0;
            } else if (currentFocus == MENU_MODE) {
                tempModeEsc = !tempModeEsc;
            } else if (currentFocus == MENU_BT) {
                // 任意方向旋转均切换临时预览状态
                tempBtState = !tempBtState;
            } else if (currentFocus == MENU_WIFI) {
                // 任意方向旋转均切换临时预览状态
                tempWifiState = !tempWifiState;
            }
        }
    }
}

void DisplayUI::render() {
    u8g2->clearBuffer();
    
    // ================== 头部状态栏 (电量/功率) ==================
    // 使用带边框的像素字体，或者数字更清晰的字体
    u8g2->setFont(u8g2_font_profont11_tr);
    u8g2->setDrawColor(1);
    
    // 左上角: 电池电压
    u8g2->setCursor(2, 9);
    u8g2->print("BAT:");
    u8g2->print(battVoltage, 1);
    u8g2->print("V");
    
    // 右上角: 实时功率 (V * A)
    float pwr = voltage * current;
    char pwrStr[16];
    snprintf(pwrStr, sizeof(pwrStr), "PWR:%.1fW", pwr);
    int pwrWidth = u8g2->getStrWidth(pwrStr);
    u8g2->setCursor(126 - pwrWidth, 9);
    u8g2->print(pwrStr);

    // 头部下划线 (分隔线)
    u8g2->drawLine(0, 11, 128, 11);

    // ================== 菜单列表 ==================
    // MiaoUI 风格：垂直列表，选中的项有圆角反色框
    const char* labels[] = {"Speed", "Mode", "Bluetooth", "Hotspot"};
    
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

        // 在 Speed 文本后方显示短进度条
        if (i == MENU_SPEED) {
            const int barX = 42;
            const int barY = y + 7;
            const int barW = 30;
            const int barH = 4;
            int fillW = (progress * (barW - 2)) / 100;
            if (fillW < 0) fillW = 0;
            if (fillW > barW - 2) fillW = barW - 2;
            u8g2->drawFrame(barX, barY, barW, barH);
            if (fillW > 0) {
                u8g2->drawBox(barX + 1, barY + 1, fillW, barH - 2);
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
    
    u8g2->sendBuffer();
}
// display ui