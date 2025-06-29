#pragma once

#include <stdbool.h>
#include <stdint.h>

// 初始化OLED显示模块
bool oled_init(void);

// 更新并显示心电波形和心率
void oled_update_ecg_display(int* ecg_data, uint16_t data_position, uint8_t heart_rate, bool leads_connected);

// 显示信息消息
void oled_show_message(const char* message);

// 显示状态信息
void oled_show_status(const char* status);
