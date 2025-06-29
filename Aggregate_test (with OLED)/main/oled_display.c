#include "oled_display.h"
#include "ssd1306.h"
#include "esp_log.h"
#include <stdio.h>
#include <string.h>
#include "hr_calculation.h"

static const char *TAG = "OLED";

// 波形显示参数
#define GRAPH_X          0       // 波形图X起始位置
#define GRAPH_Y          16      // 波形图Y起始位置
#define GRAPH_WIDTH      128     // 波形图宽度
#define GRAPH_HEIGHT     32      // 波形图高度
#define GRAPH_BASELINE   (GRAPH_Y + GRAPH_HEIGHT/2) // 波形基线位置

// 初始化OLED显示模块
bool oled_init(void) {
    if (!ssd1306_init()) {
        ESP_LOGE(TAG, "OLED initialization failed");
        return false;
    }
    
    // 显示欢迎信息
    ssd1306_clear();
    ssd1306_draw_string(0, 0, "ECG Monitor");
    ssd1306_draw_string(0, 16, "Initializing...");
    ssd1306_display();
    
    ESP_LOGI(TAG, "OLED display module initialized");
    return true;
}

// 更新并显示心电波形和心率
void oled_update_ecg_display(int* ecg_data, uint16_t data_position, uint8_t heart_rate, bool leads_connected) {
    if (ecg_data == NULL) {
        return;
    }
    
    // 清除屏幕
    ssd1306_clear();
    
    // 绘制标题和心率
    char heart_rate_str[16];
    if (heart_rate > 0) {
        snprintf(heart_rate_str, sizeof(heart_rate_str), "HR: %d BPM", heart_rate);
    } else {
        snprintf(heart_rate_str, sizeof(heart_rate_str), "HR: ---");
    }
    ssd1306_draw_string(0, 0, heart_rate_str);
    
    // 显示导联状态
    if (!leads_connected) {
        ssd1306_draw_string(64, 0, "LEADS OFF");
    }
    
    // 绘制波形框
    ssd1306_draw_rect(GRAPH_X, GRAPH_Y, GRAPH_WIDTH, GRAPH_HEIGHT, true);
    
    // 绘制基准线
    ssd1306_draw_hline(GRAPH_X, GRAPH_BASELINE, GRAPH_WIDTH, true);
    
    // 如果导联未连接，不绘制波形
    if (!leads_connected) {
        ssd1306_draw_string(GRAPH_X + 16, GRAPH_BASELINE - 4, "Check Leads");
        ssd1306_display();
        return;
    }
    
    // 绘制波形
    const uint16_t buffer_size = HR_BUFFER_SIZE; // 从hr_calculation.h获取
    const float scale_factor = 0.01;  // 增大缩放因子，使波形更明显
    
    // 记录前一个点的位置，用于绘制线段
    int prev_x = GRAPH_X + 1;
    int prev_y = GRAPH_BASELINE;
    bool first_point = true;
    
    for (int i = 0; i < GRAPH_WIDTH - 2; i++) {
        // 计算要读取的缓冲区索引
        uint16_t index = (data_position - GRAPH_WIDTH + i + buffer_size) % buffer_size;
        
        // 将ADC值映射到显示区域
        int adc_value = ecg_data[index];
        int y_pos = GRAPH_BASELINE - (int)(adc_value * scale_factor);
        
        // 限制Y值在绘图区域内
        if (y_pos < GRAPH_Y + 1) y_pos = GRAPH_Y + 1;
        if (y_pos > GRAPH_Y + GRAPH_HEIGHT - 2) y_pos = GRAPH_Y + GRAPH_HEIGHT - 2;
        
        int current_x = GRAPH_X + 1 + i;
        
        // 如果不是第一个点，则绘制与前一个点的连线
        if (!first_point) {
            // 绘制从前一个点到当前点的线段
            if (abs(prev_y - y_pos) <= 1) {
                // 相邻点高度差不大，直接绘制像素点
                ssd1306_draw_pixel(current_x, y_pos, true);
            } else {
                // 相邻点高度差较大，绘制直线连接
                ssd1306_draw_line(prev_x, prev_y, current_x, y_pos, true);
            }
        } else {
            // 第一个点直接绘制
            ssd1306_draw_pixel(current_x, y_pos, true);
            first_point = false;
        }
        
        // 更新前一个点的位置
        prev_x = current_x;
        prev_y = y_pos;
    }
    
    // 刷新显示
    ssd1306_display();
}

// 显示信息消息
void oled_show_message(const char* message) {
    if (message == NULL) {
        return;
    }
    
    ssd1306_clear();
    ssd1306_draw_string(0, 0, "ECG Monitor");
    ssd1306_draw_string(0, 16, message);
    ssd1306_display();
}

// 显示状态信息
void oled_show_status(const char* status) {
    if (status == NULL) {
        return;
    }
    
    // 只更新状态行
    ssd1306_draw_string(0, 56, status); // 底部状态行
    ssd1306_display();
}
