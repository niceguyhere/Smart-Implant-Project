#pragma once

#include <stdbool.h>
#include "driver/gpio.h"

// GPIO定义
#define PIN_AD8232_OUT       3     // AD8232模拟输出引脚
#define PIN_AD8232_LOD_P     21    // AD8232 LOD+引脚（LA电极脱落检测）
// SDN引脚不再使用，保持悬空

// 电极状态检测间隔（毫秒）
#define ELECTRODE_CHECK_INTERVAL_MS 1000   // 1Hz采样率

// 电极状态枚举
typedef enum {
    ELECTRODE_NORMAL = 0, // 电极连接正常
    ELECTRODE_OFF    = 1  // 电极脱落
} ElectrodeStatus_t;

// 初始化引脚控制模块
bool pin_control_init(void);

// 获取电极状态
// 返回值: ElectrodeStatus_t 枚举值
ElectrodeStatus_t get_electrode_status(void);

// AD8232电源控制功能已移除，SDN引脚保持悬空

// 设置调试LED状态
void pin_set_debug_led(bool state);

// 电极状态监测任务
void electrode_status_task(void *arg);
