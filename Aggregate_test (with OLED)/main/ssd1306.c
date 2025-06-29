#include "ssd1306.h"
#include "esp_log.h"
#include <string.h>
#include "pin_control.h"


static const char *TAG = "SSD1306";

// 字体表 5x8
// ASCII字符集, 从空格 (0x20) 到 '~' (0x7E)
static const uint8_t font5x8[] = {
    0x00, 0x00, 0x00, 0x00, 0x00, // u7a7au683c
    0x00, 0x00, 0x5F, 0x00, 0x00, // !
    0x00, 0x07, 0x00, 0x07, 0x00, // "
    0x14, 0x7F, 0x14, 0x7F, 0x14, // #
    0x24, 0x2A, 0x7F, 0x2A, 0x12, // $
    0x23, 0x13, 0x08, 0x64, 0x62, // %
    0x36, 0x49, 0x56, 0x20, 0x50, // &
    0x00, 0x08, 0x07, 0x03, 0x00, // '
    0x00, 0x1C, 0x22, 0x41, 0x00, // (
    0x00, 0x41, 0x22, 0x1C, 0x00, // )
    0x2A, 0x1C, 0x7F, 0x1C, 0x2A, // *
    0x08, 0x08, 0x3E, 0x08, 0x08, // +
    0x00, 0x80, 0x70, 0x30, 0x00, // ,
    0x08, 0x08, 0x08, 0x08, 0x08, // -
    0x00, 0x00, 0x60, 0x60, 0x00, // .
    0x20, 0x10, 0x08, 0x04, 0x02, // /
    0x3E, 0x51, 0x49, 0x45, 0x3E, // 0
    0x00, 0x42, 0x7F, 0x40, 0x00, // 1
    0x72, 0x49, 0x49, 0x49, 0x46, // 2
    0x21, 0x41, 0x49, 0x4D, 0x33, // 3
    0x18, 0x14, 0x12, 0x7F, 0x10, // 4
    0x27, 0x45, 0x45, 0x45, 0x39, // 5
    0x3C, 0x4A, 0x49, 0x49, 0x31, // 6
    0x41, 0x21, 0x11, 0x09, 0x07, // 7
    0x36, 0x49, 0x49, 0x49, 0x36, // 8
    0x46, 0x49, 0x49, 0x29, 0x1E, // 9
    0x00, 0x00, 0x14, 0x00, 0x00, // :
    0x00, 0x40, 0x34, 0x00, 0x00, // ;
    0x00, 0x08, 0x14, 0x22, 0x41, // <
    0x14, 0x14, 0x14, 0x14, 0x14, // =
    0x41, 0x22, 0x14, 0x08, 0x00, // >
    0x02, 0x01, 0x59, 0x09, 0x06, // ?
    0x3E, 0x41, 0x5D, 0x59, 0x4E, // @
    0x7C, 0x12, 0x11, 0x12, 0x7C, // A
    0x7F, 0x49, 0x49, 0x49, 0x36, // B
    0x3E, 0x41, 0x41, 0x41, 0x22, // C
    0x7F, 0x41, 0x41, 0x41, 0x3E, // D
    0x7F, 0x49, 0x49, 0x49, 0x41, // E
    0x7F, 0x09, 0x09, 0x09, 0x01, // F
    0x3E, 0x41, 0x41, 0x51, 0x73, // G
    0x7F, 0x08, 0x08, 0x08, 0x7F, // H
    0x00, 0x41, 0x7F, 0x41, 0x00, // I
    0x20, 0x40, 0x41, 0x3F, 0x01, // J
    0x7F, 0x08, 0x14, 0x22, 0x41, // K
    0x7F, 0x40, 0x40, 0x40, 0x40, // L
    0x7F, 0x02, 0x1C, 0x02, 0x7F, // M
    0x7F, 0x04, 0x08, 0x10, 0x7F, // N
    0x3E, 0x41, 0x41, 0x41, 0x3E, // O
    0x7F, 0x09, 0x09, 0x09, 0x06, // P
    0x3E, 0x41, 0x51, 0x21, 0x5E, // Q
    0x7F, 0x09, 0x19, 0x29, 0x46, // R
    0x26, 0x49, 0x49, 0x49, 0x32, // S
    0x01, 0x01, 0x7F, 0x01, 0x01, // T
    0x3F, 0x40, 0x40, 0x40, 0x3F, // U
    0x1F, 0x20, 0x40, 0x20, 0x1F, // V
    0x3F, 0x40, 0x38, 0x40, 0x3F, // W
    0x63, 0x14, 0x08, 0x14, 0x63, // X
    0x07, 0x08, 0x70, 0x08, 0x07, // Y
    0x61, 0x51, 0x49, 0x45, 0x43, // Z
    0x00, 0x7F, 0x41, 0x41, 0x00, // [
    0x02, 0x04, 0x08, 0x10, 0x20, // ASCII 92
    0x00, 0x41, 0x41, 0x7F, 0x00, // ]
    0x04, 0x02, 0x01, 0x02, 0x04, // ^
    0x40, 0x40, 0x40, 0x40, 0x40, // _
    0x00, 0x03, 0x07, 0x08, 0x00, // `
    0x20, 0x54, 0x54, 0x54, 0x78, // a
    0x7F, 0x48, 0x44, 0x44, 0x38, // b
    0x38, 0x44, 0x44, 0x44, 0x20, // c
    0x38, 0x44, 0x44, 0x48, 0x7F, // d
    0x38, 0x54, 0x54, 0x54, 0x18, // e
    0x08, 0x7E, 0x09, 0x01, 0x02, // f
    0x18, 0xA4, 0xA4, 0xA4, 0x7C, // g
    0x7F, 0x08, 0x04, 0x04, 0x78, // h
    0x00, 0x44, 0x7D, 0x40, 0x00, // i
    0x40, 0x80, 0x84, 0x7D, 0x00, // j
    0x7F, 0x10, 0x28, 0x44, 0x00, // k
    0x00, 0x41, 0x7F, 0x40, 0x00, // l
    0x7C, 0x04, 0x78, 0x04, 0x78, // m
    0x7C, 0x08, 0x04, 0x04, 0x78, // n
    0x38, 0x44, 0x44, 0x44, 0x38, // o
    0xFC, 0x24, 0x24, 0x24, 0x18, // p
    0x18, 0x24, 0x24, 0x28, 0xFC, // q
    0x7C, 0x08, 0x04, 0x04, 0x08, // r
    0x48, 0x54, 0x54, 0x54, 0x20, // s
    0x04, 0x3F, 0x44, 0x40, 0x20, // t
    0x3C, 0x40, 0x40, 0x20, 0x7C, // u
    0x1C, 0x20, 0x40, 0x20, 0x1C, // v
    0x3C, 0x40, 0x30, 0x40, 0x3C, // w
    0x44, 0x28, 0x10, 0x28, 0x44, // x
    0x1C, 0xA0, 0xA0, 0xA0, 0x7C, // y
    0x44, 0x64, 0x54, 0x4C, 0x44, // z
    0x00, 0x08, 0x36, 0x41, 0x00, // {
    0x00, 0x00, 0x77, 0x00, 0x00, // |
    0x00, 0x41, 0x36, 0x08, 0x00, // }
    0x02, 0x01, 0x02, 0x04, 0x02  // ~
};

// SSD1306u521du59cbu5316u547du4ee4
static const uint8_t ssd1306_init_cmd[] = {
    0xAE,       // u5173u95edu663eu793a
    0xD5, 0x80, // u8bbeu7f6eu663eu793au65f6u949fu5206u9891
    0xA8, 0x3F, // u8bbeu7f6eu591au8defu590du7528u6bd4
    0xD3, 0x00, // u8bbeu7f6eu663eu793au504fu79fb
    0x40,       // u8bbeu7f6eu663eu793au5f00u59cbu884c [5:0]
    0x8D, 0x14, // u5145u7535u6cf0u8fbeu7535u8def
    0x20, 0x00, // u8bbeu7f6eu5185u5b58u5bfbu5740u6a21u5f0fuff080x00u6c34u5e73u5bfbu5740uff0c0x01u5782u76f4u5bfbu5740uff09
    0xA1,       // u8bbeu7f6eu5217u91cdu65b0u6620u5c04uff0c0xA0u6b63u5e38uff0c0xA1u5e94u7528u5217u5730u5740 0 u662fu6bb5u91cdu65b0u6620u5c04
    0xC8,       // u8bbeu7f6ecomu8f93u51fau626bu63cfu65b9u5411uff0cu0xC0u6b63u5e38uff0c0xC8u5e94u7528comu8f93u51fau626bu63cfu65b9u5411
    0xDA, 0x12, // u8bbeu7f6ecomu5f15u811au786cu4ef6u914du7f6e
    0x81, 0xCF, // u8bbeu7f6eu5bf9u6bd4u5ea6
    0xD9, 0xF1, // u8bbeu7f6eu9884u5145u7535u5468u671f
    0xDB, 0x40, // u8bbeu7f6eVCOMHu7535u5e73
    0xA4,       // u51fau5c55u5c4fu5e55u4e0au7684u5185u5bb9uff0c0xA4u6b63u5e38uff0c0xA5u6574u4e2au663eu793au4eae
    0xA6,       // u8bbeu7f6eu6b63u5e38/u53cdu8f6cu663eu793auff0c0xA6u6b63u5e38uff0c0xA7u53cdu8f6c
    0xAF        // u6253u5f00u663eu793a
};

// u5c4fu5e55u7f13u51b2u533a (128x64 / 8 = 1024 u5b57u8282)
static uint8_t ssd1306_buffer[SSD1306_WIDTH * SSD1306_HEIGHT / 8];
static uint8_t text_size = 1; // u5b57u4f53u7f29u653eu6bd4u4f8b
static uint8_t rotation = 0;  // u65cbu8f6cu89d2u5ea6 (0, 1, 2, 3)

// I2C发送命令
static esp_err_t ssd1306_command(uint8_t command) {
    uint8_t data[2] = {0x00, command}; // 控制字节 0x00, 命令字节
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (SSD1306_I2C_ADDR << 1) | I2C_MASTER_WRITE, true);
    i2c_master_write(cmd, data, 2, true);
    i2c_master_stop(cmd);
    esp_err_t ret = i2c_master_cmd_begin(I2C_MASTER_NUM, cmd, I2C_MASTER_TIMEOUT_MS / portTICK_PERIOD_MS);
    i2c_cmd_link_delete(cmd);
    return ret;
}

// I2C发送数据
static esp_err_t ssd1306_data(uint8_t *data, size_t size) {
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (SSD1306_I2C_ADDR << 1) | I2C_MASTER_WRITE, true);
    i2c_master_write_byte(cmd, 0x40, true); // 控制字节 0x40 表示数据
    i2c_master_write(cmd, data, size, true);
    i2c_master_stop(cmd);
    esp_err_t ret = i2c_master_cmd_begin(I2C_MASTER_NUM, cmd, I2C_MASTER_TIMEOUT_MS / portTICK_PERIOD_MS);
    i2c_cmd_link_delete(cmd);
    return ret;
}

// 初始化SSD1306 OLED显示器
bool ssd1306_init(void) {
    // 配置I2C
    i2c_config_t i2c_conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = PIN_OLED_SDA,
        .scl_io_num = PIN_OLED_SCL,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = I2C_MASTER_FREQ_HZ
    };
    
    esp_err_t ret = i2c_param_config(I2C_MASTER_NUM, &i2c_conf);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "I2C parameter configuration failed");
        return false;
    }
    
    ret = i2c_driver_install(I2C_MASTER_NUM, i2c_conf.mode, 0, 0, 0);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "I2C driver installation failed");
        return false;
    }
    
    // 发送初始化命令序列
    for (uint8_t i = 0; i < sizeof(ssd1306_init_cmd); i++) {
        ret = ssd1306_command(ssd1306_init_cmd[i]);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "OLED command failed: %d", ret);
            return false;
        }
    }
    
    // 清空缓冲区和显示
    memset(ssd1306_buffer, 0, sizeof(ssd1306_buffer));
    ssd1306_display();
    
    ESP_LOGI(TAG, "SSD1306 OLED initialized");
    return true;
}

// 清空OLED显示内容
void ssd1306_clear(void) {
    memset(ssd1306_buffer, 0, sizeof(ssd1306_buffer));
}

// 刷新显示内容到OLED屏幕
void ssd1306_display(void) {
    // 设置列地址范围
    ssd1306_command(0x21); // 设置列地址
    ssd1306_command(0);    // 起始列地址
    ssd1306_command(127);  // 结束列地址
    
    // 设置页地址范围
    ssd1306_command(0x22); // 设置页地址
    ssd1306_command(0);    // 起始页地址
    ssd1306_command(7);    // 结束页地址
    
    // 发送整个缓冲区
    ssd1306_data(ssd1306_buffer, sizeof(ssd1306_buffer));
}

// 在指定位置绘制像素点
void ssd1306_draw_pixel(int16_t x, int16_t y, bool color) {
    // 处理超出范围的坐标
    if (x < 0 || x >= SSD1306_WIDTH || y < 0 || y >= SSD1306_HEIGHT) {
        return;
    }
    
    // 处理旋转
    int16_t temp_x = x;
    int16_t temp_y = y;
    
    switch (rotation) {
        case 1: // 90度旋转
            temp_x = y;
            temp_y = SSD1306_WIDTH - 1 - x;
            break;
        case 2: // 180度旋转
            temp_x = SSD1306_WIDTH - 1 - x;
            temp_y = SSD1306_HEIGHT - 1 - y;
            break;
        case 3: // 270度旋转
            temp_x = SSD1306_HEIGHT - 1 - y;
            temp_y = x;
            break;
    }
    
    // 计算缓冲区索引和位位置
    uint16_t index = temp_x + (temp_y / 8) * SSD1306_WIDTH;
    uint8_t bit_position = temp_y % 8;
    
    // 设置或清除像素
    if (color) {
        ssd1306_buffer[index] |= (1 << bit_position); // 设置像素
    } else {
        ssd1306_buffer[index] &= ~(1 << bit_position); // 清除像素
    }
}

// 绘制水平线
void ssd1306_draw_hline(int16_t x, int16_t y, int16_t w, bool color) {
    for (int16_t i = 0; i < w; i++) {
        ssd1306_draw_pixel(x + i, y, color);
    }
}

// 绘制垂直线
void ssd1306_draw_vline(int16_t x, int16_t y, int16_t h, bool color) {
    for (int16_t i = 0; i < h; i++) {
        ssd1306_draw_pixel(x, y + i, color);
    }
}

// 绘制矩形
void ssd1306_draw_rect(int16_t x, int16_t y, int16_t w, int16_t h, bool color) {
    ssd1306_draw_hline(x, y, w, color);
    ssd1306_draw_hline(x, y + h - 1, w, color);
    ssd1306_draw_vline(x, y, h, color);
    ssd1306_draw_vline(x + w - 1, y, h, color);
}

// 绘制填充矩形
void ssd1306_fill_rect(int16_t x, int16_t y, int16_t w, int16_t h, bool color) {
    for (int16_t i = 0; i < h; i++) {
        ssd1306_draw_hline(x, y + i, w, color);
    }
}

// 设置字体大小
void ssd1306_set_text_size(uint8_t size) {
    text_size = (size > 0) ? size : 1;
}

// 设置显示方向
void ssd1306_set_rotation(uint8_t r) {
    rotation = r % 4; // 仅支持0-3四个方向
}

// 绘制单个字符
static void ssd1306_draw_char(int16_t x, int16_t y, unsigned char c, bool color) {
    // 处理超出可见ASCII范围的字符
    if (c < 0x20 || c > 0x7E) {
        c = '?'; // 替换为问号
    }
    
    // 字符在字体表中的索引
    c -= 0x20; // 空格字符ASCII 0x20 对应字体表中的索引0
    
    // 绘制字符
    for (int8_t i = 0; i < 5; i++) { // 每个字符宽5个像素
        uint8_t line = font5x8[c * 5 + i];
        
        for (int8_t j = 0; j < 8; j++) { // 每个字符高8个像素
            if (line & (1 << j)) {
                // 处理字体缩放
                if (text_size == 1) {
                    ssd1306_draw_pixel(x + i, y + j, color);
                } else {
                    ssd1306_fill_rect(x + i * text_size, y + j * text_size, text_size, text_size, color);
                }
            }
        }
    }
}

// 在指定位置显示字符串
void ssd1306_draw_string(uint8_t x, uint8_t y, const char *str) {
    int16_t cursor_x = x;
    int16_t cursor_y = y;
    
    // 逐字符绘制
    while (*str) {
        ssd1306_draw_char(cursor_x, cursor_y, *str, true);
        
        // 移动光标
        cursor_x += 6 * text_size; // 5像素字符宽度 + 1像素间距
        
        // 处理换行
        if (*str == '\n' || cursor_x >= (SSD1306_WIDTH - 6 * text_size)) {
            cursor_x = x;
            cursor_y += 8 * text_size; // 8像素字符高度
        }
        
        str++;
    }
}

// 绘制线段 (使用Bresenham算法)
void ssd1306_draw_line(int16_t x0, int16_t y0, int16_t x1, int16_t y1, bool color) {
    // 线段的斜率绝对值大于1时需要交换坐标轴
    bool steep = abs(y1 - y0) > abs(x1 - x0);
    
    if (steep) {
        // 交换x和y坐标
        int16_t temp = x0;
        x0 = y0;
        y0 = temp;
        
        temp = x1;
        x1 = y1;
        y1 = temp;
    }
    
    // 如果线从右向左绘制，交换起点和终点
    if (x0 > x1) {
        int16_t temp = x0;
        x0 = x1;
        x1 = temp;
        
        temp = y0;
        y0 = y1;
        y1 = temp;
    }
    
    int16_t dx = x1 - x0;
    int16_t dy = abs(y1 - y0);
    int16_t err = dx / 2;
    int16_t ystep;
    
    if (y0 < y1) {
        ystep = 1;
    } else {
        ystep = -1;
    }
    
    for (; x0 <= x1; x0++) {
        if (steep) {
            // 如果交换了坐标轴，需要反过来绘制
            ssd1306_draw_pixel(y0, x0, color);
        } else {
            ssd1306_draw_pixel(x0, y0, color);
        }
        
        err -= dy;
        if (err < 0) {
            y0 += ystep;
            err += dx;
        }
    }
}

// 绘制位图
void ssd1306_draw_bitmap(int16_t x, int16_t y, const uint8_t *bitmap, int16_t w, int16_t h, bool color) {
    for (int16_t j = 0; j < h; j++) {
        for (int16_t i = 0; i < w; i++) {
            if (bitmap[i + (j/8)*w] & (1 << (j % 8))) {
                ssd1306_draw_pixel(x + i, y + j, color);
            }
        }
    }
}
