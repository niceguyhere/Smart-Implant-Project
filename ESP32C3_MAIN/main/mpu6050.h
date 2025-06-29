#pragma once

#include "driver/i2c.h"
#include <stdint.h>
#include <stdbool.h>

// MPU6050相关定义
#define MPU6050_I2C_ADDR       0x68
#define MPU6050_SDA_GPIO       8
#define MPU6050_SCL_GPIO       9
#define MPU6050_I2C_PORT       I2C_NUM_0

// 睡眠检测相关配置
#define SLEEP_DETECT_THRESHOLD    100     // 动作变化阈值，低于此值视为静止
#define SLEEP_DETECT_TIME_MIN     30      // 检测睡眠的时间阈值(分钟)
#define WAKE_DETECT_TIME_MIN      10      // 检测唤醒的时间阈值(分钟)
#define MOTION_HISTORY_SIZE       60      // 运动历史记录数量

// 初始化MPU6050
esp_err_t mpu6050_init(void);

// 读取原始传感器数据
void mpu6050_read_raw(int16_t *accel_x, int16_t *accel_y, int16_t *accel_z, 
                     int16_t *gyro_x, int16_t *gyro_y, int16_t *gyro_z);

// 计算温度（摄氏度）
float mpu6050_read_temp(void);

// 获取当前睡眠状态
bool mpu6050_is_sleeping(void);

// 设置MPU6050的采样率
esp_err_t mpu6050_set_sample_rate(uint8_t rate_hz);

// 开启/关闭陀螺仪
esp_err_t mpu6050_enable_gyro(bool enable);

// 更新运动状态
void mpu6050_update_motion_state(int16_t accel_x, int16_t accel_y, int16_t accel_z,
                               int16_t gyro_x, int16_t gyro_y, int16_t gyro_z);
