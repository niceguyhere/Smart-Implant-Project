#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "config.h"

// 心率计算相关参数
#define ECG_SAMPLE_RATE         ECG_SAMPLE_RATE_HZ
#define HR_DETECTION_THRESHOLD   1800  // 心率检测阈值
#define HR_MIN_DISTANCE_SAMPLES  (ECG_SAMPLE_RATE_HZ / 4) // Approx 240 BPM max (125 samples at 500Hz)    // 心跳间最小距离(采样点)
#define HR_BUFFER_SIZE           600   // 3秒的数据缓冲区大小

// 初始化心率计算模块
bool hr_init(void);

// 输入新的ECG采样点并处理
bool hr_process_sample(int ecg_value);

// 获取当前计算的心率值 (bpm)
uint8_t hr_get_rate(void);

// 获取波形缓冲区数据，用于显示
int* hr_get_waveform_buffer(void);

// 获取最新波形点位置
uint16_t hr_get_waveform_position(void);

// 检查导联状态
bool hr_check_leads_connected(void);
