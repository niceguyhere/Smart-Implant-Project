#pragma once

#include <stdbool.h>
#include "esp_adc/adc_oneshot.h"
#include "config.h"

/**
 * @brief 初始化NTC温度传感器
 * @return 初始化是否成功
 */
bool ntc_sensor_init(void);

/**
 * @brief 读取NTC1的温度值（GPIO0）
 * @return 温度值，单位为摄氏度
 */
float ntc_read_temp1(void);

/**
 * @brief 读取NTC2的温度值（GPIO2）
 * @return 温度值，单位为摄氏度
 */
float ntc_read_temp2(void);

/**
 * @brief 温度监测任务，定期读取并打印两个NTC的温度
 */
void ntc_temp_monitor_task(void *pvParameters);
