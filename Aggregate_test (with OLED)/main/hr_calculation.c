#include "hr_calculation.h"
#include "pin_control.h"
#include "esp_log.h"
#include <string.h>

static const char *TAG = "HR_CALC";

// 缓冲区用于存储ECG数据
static int ecg_buffer[HR_BUFFER_SIZE];
static uint16_t buffer_index = 0;

// R波峰检测相关变量
static uint32_t last_peak_time = 0;
static uint32_t current_time = 0;
static uint8_t heart_rate = 0;
static int peak_count = 0;
static uint32_t rr_intervals[8] = {0}; // 存储最近8个RR间隔
static uint8_t rr_index = 0;

// 初始化心率计算模块
bool hr_init(void) {
    // 清空缓冲区
    memset(ecg_buffer, 0, sizeof(ecg_buffer));
    buffer_index = 0;
    heart_rate = 0;
    peak_count = 0;
    last_peak_time = 0;
    current_time = 0;
    
    ESP_LOGI(TAG, "Heart rate calculation module initialized");
    return true;
}

// 简单的移动平均滤波
#define FILTER_SIZE 5  // 使用5点移动平均滤波
static int filter_buffer[FILTER_SIZE] = {0};
static int filter_index = 0;

// 应用移动平均滤波
static int apply_filter(int new_value) {
    // 将新值添加到滤波器缓冲区
    filter_buffer[filter_index] = new_value;
    filter_index = (filter_index + 1) % FILTER_SIZE;
    
    // 计算平均值
    int sum = 0;
    for (int i = 0; i < FILTER_SIZE; i++) {
        sum += filter_buffer[i];
    }
    
    return sum / FILTER_SIZE;
}

// 输入新的ECG采样点并处理
bool hr_process_sample(int ecg_value) {
    // 应用滤波器
    int filtered_value = apply_filter(ecg_value);
    
    // 存储已滤波的ECG数据到缓冲区
    ecg_buffer[buffer_index] = filtered_value;
    buffer_index = (buffer_index + 1) % HR_BUFFER_SIZE;
    
    // 增加时间计数器
    current_time++;
    
    // 简单的峰值检测算法 - 基于阈值
    // 检查当前采样点是否超过阈值，且与上一个峰值之间有足够的距离
    if (ecg_value > HR_DETECTION_THRESHOLD && 
        (current_time - last_peak_time) > HR_MIN_DISTANCE_SAMPLES) {
        
        // 计算RR间隔（两个相邻R波之间的间隔）
        uint32_t rr_interval = current_time - last_peak_time;
        
        // 存储RR间隔用于平均计算
        if (last_peak_time > 0) { // 不是第一个峰值
            rr_intervals[rr_index] = rr_interval;
            rr_index = (rr_index + 1) % 8;
            peak_count++;
        }
        
        // 更新上次峰值时间
        last_peak_time = current_time;
        
        // 计算心率 (至少有3个RR间隔后)
        if (peak_count >= 3) {
            // 计算平均RR间隔
            uint32_t sum = 0;
            uint8_t count = (peak_count > 8) ? 8 : peak_count;
            
            for (int i = 0; i < count; i++) {
                sum += rr_intervals[i];
            }
            
            uint32_t avg_rr = sum / count;
            
            // 转换为BPM (心跳/分钟)
            // BPM = 60 * 采样率 / RR间隔
            heart_rate = (60 * ECG_SAMPLE_RATE) / avg_rr;
            
            // 确保心率在合理范围内
            if (heart_rate < 40) heart_rate = 0; // 可能是假波形
            if (heart_rate > 200) heart_rate = 0; // 可能是噪声
        }
        
        return true; // 检测到一个新的R波
    }
    
    return false;
}

// 获取当前计算的心率值 (bpm)
uint8_t hr_get_rate(void) {
    return heart_rate;
}

// 获取波形缓冲区数据，用于显示
int* hr_get_waveform_buffer(void) {
    return ecg_buffer;
}

// 获取最新波形点位置
uint16_t hr_get_waveform_position(void) {
    return buffer_index;
}

// 检查导联状态
bool hr_check_leads_connected(void) {
    return pin_get_lod_status();
}
