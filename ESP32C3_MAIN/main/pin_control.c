#include "pin_control.h"
#include "esp_log.h"
#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "sd_card.h" // For sd_add_lead_data
#include "config.h"  // For ELECTRODE_CHECK_INTERVAL_MS

#ifndef CONFIG_LOG_MAXIMUM_LEVEL
#define CONFIG_LOG_MAXIMUM_LEVEL 3  // ESP_LOG_INFO
#endif

static const char *TAG = "PIN_CTRL";

// 初始化引脚控制模块
bool pin_control_init(void) {
    // 配置AD8232 OUT引脚 (模拟输入，不需要GPIO配置)
    
    // 配置AD8232 LOD+引脚为输入
    gpio_config_t io_conf_lod_p = {
        .pin_bit_mask = (1ULL << PIN_AD8232_LOD_P),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    gpio_config(&io_conf_lod_p);
    
    // SDN引脚不再配置和控制，保持悬空
    
    ESP_LOGI(TAG, "Pin control module initialized for single electrode detection (GPIO%d)", PIN_AD8232_LOD_P);
    return true;
}

// 获取电极状态
// 返回值: ElectrodeStatus_t 枚举值
ElectrodeStatus_t get_electrode_status(void) {
    // PIN_AD8232_LOD_P (GPIO21) 高电平代表电极状态脱落，低电平代表电极状态正常
    if (gpio_get_level(PIN_AD8232_LOD_P) == 1) {
        return ELECTRODE_OFF;
    } else {
        return ELECTRODE_NORMAL;
    }
}

// AD8232电源控制功能已移除，SDN引脚保持悬空

// 设置调试LED状态
void pin_set_debug_led(bool state) {
    // ESP32-C3 SuperMini没有专用的调试LED，此函数仅作为扩展功能预留
    // 如果需要，可以在这里添加实际的LED控制代码
}

// 电极状态监测任务 (moved from main.c and adapted)
void electrode_status_task(void *arg) {
    ESP_LOGI(TAG, "Electrode status monitoring task started (GPIO%d)", PIN_AD8232_LOD_P);
    
    ElectrodeStatus_t current_status;
    
    while (1) {
        current_status = get_electrode_status();
        
        // 添加到缓冲区 (true for OFF, false for NORMAL)
        // This will be updated later when sd_card module is refactored for ElectrodeStatus_t
        sd_add_lead_data(current_status == ELECTRODE_OFF); 
        
        // 打印电极状态
        if (current_status == ELECTRODE_NORMAL) {
            // Using ESP_LOGI for consistency, but printf is also fine for quick debug
            ESP_LOGI(TAG, "Electrode Status: Normal");
        } else {
            ESP_LOGE(TAG, "Electrode Status: OFF"); // Use ERROR level for detached status for visibility
        }
        
        // 每秒更新一次 (ELECTRODE_CHECK_INTERVAL_MS is defined in config.h or pin_control.h)
        vTaskDelay(pdMS_TO_TICKS(ELECTRODE_CHECK_INTERVAL_MS));
    }
}
