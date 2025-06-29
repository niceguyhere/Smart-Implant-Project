#pragma once

#include <stdbool.h>
#include "driver/gpio.h"

// GPIO定义
#define PIN_AD8232_OUT       3     // AD8232模拟输出引脚
#define PIN_AD8232_LOD       10    // AD8232 LOD+引脚（导联脱落检测）
// SDN引脚不再使用，保持悬空

// 初始化引脚控制模块
bool pin_control_init(void);

// 获取导联脱落检测状态
// 返回值: true=导联已连接，false=导联脱落
bool pin_get_lod_status(void);

// AD8232电源控制功能已移除，SDN引脚保持悬空

// 设置调试LED状态
void pin_set_debug_led(bool state);
