#pragma once

#include "driver/i2c.h"
#include <stdint.h>
#include <stdbool.h>

// SSD1306屏幕参数
#define SSD1306_WIDTH              128
#define SSD1306_HEIGHT             64
#define SSD1306_I2C_ADDR           0x3C    // 常见的OLED地址，部分模块可能是0x3D

// I2C通信参数
#define I2C_MASTER_NUM             I2C_NUM_0
#define I2C_MASTER_FREQ_HZ          400000  // 400 KHz
#define I2C_MASTER_TIMEOUT_MS       1000

// 初始化SSD1306 OLED显示器
bool ssd1306_init(void);

// 清空OLED显示内容
void ssd1306_clear(void);

// 刷新显示内容到OLED屏幕
void ssd1306_display(void);

// 在指定位置显示字符串
void ssd1306_draw_string(uint8_t x, uint8_t y, const char *str);

// 在指定位置绘制像素点
void ssd1306_draw_pixel(int16_t x, int16_t y, bool color);

// 绘制水平线
void ssd1306_draw_hline(int16_t x, int16_t y, int16_t w, bool color);

// 绘制垂直线
void ssd1306_draw_vline(int16_t x, int16_t y, int16_t h, bool color);

// 绘制矩形
void ssd1306_draw_rect(int16_t x, int16_t y, int16_t w, int16_t h, bool color);

// 绘制填充矩形
void ssd1306_fill_rect(int16_t x, int16_t y, int16_t w, int16_t h, bool color);

// 绘制线段
void ssd1306_draw_line(int16_t x0, int16_t y0, int16_t x1, int16_t y1, bool color);

// 绘制位图
void ssd1306_draw_bitmap(int16_t x, int16_t y, const uint8_t *bitmap, int16_t w, int16_t h, bool color);

// 设置字体大小
void ssd1306_set_text_size(uint8_t size);

// 设置显示方向
void ssd1306_set_rotation(uint8_t rotation);
