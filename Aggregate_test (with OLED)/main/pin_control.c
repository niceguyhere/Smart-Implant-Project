#include "pin_control.h"
#include "esp_log.h"

static const char *TAG = "PIN_CTRL";

// 初始化引脚控制模块
bool pin_control_init(void) {
    // 配置AD8232 OUT引脚 (模拟输入，不需要GPIO配置)
    
    // 配置AD8232 LOD+引脚为输入
    gpio_config_t io_conf_lod = {
        .pin_bit_mask = (1ULL << PIN_AD8232_LOD),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    gpio_config(&io_conf_lod);
    
    // SDN引脚不再配置和控制，保持悬空
    
    ESP_LOGI(TAG, "Pin control module initialized");
    return true;
}

// 获取导联脱落检测状态
// 返回值: true=导联已连接，false=导联脱落
bool pin_get_lod_status(void) {
    // LOD+为低电平表示导联已连接，高电平表示导联脱落
    return (gpio_get_level(PIN_AD8232_LOD) == 0);
}

// AD8232电源控制功能已移除，SDN引脚保持悬空

// 设置调试LED状态
void pin_set_debug_led(bool state) {
    // ESP32-C3 SuperMini没有专用的调试LED，此函数仅作为扩展功能预留
    // 如果需要，可以在这里添加实际的LED控制代码
}
