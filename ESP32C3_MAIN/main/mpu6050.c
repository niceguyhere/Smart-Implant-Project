#include "mpu6050.h"
#include "esp_log.h"
#include "driver/i2c.h"
#include <math.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "config.h"    // 包含采样率定义

static const char *TAG = "MPU6050";

// 运动历史记录与状态变量
typedef struct {
    int16_t accel_x;
    int16_t accel_y;
    int16_t accel_z;
    int16_t gyro_x;
    int16_t gyro_y;
    int16_t gyro_z;
    uint32_t timestamp;   // 采样时间戳(秒)
} motion_sample_t;

static motion_sample_t motion_history[MOTION_HISTORY_SIZE]; // 运动历史记录
static uint8_t motion_index = 0;                           // 当前历史记录索引
static bool is_sleeping = false;                           // 当前睡眠状态
static bool gyro_enabled = true;                           // 陀螺仪状态
static uint32_t stable_start_time = 0;                     // 稳定状态开始时间
static uint32_t active_start_time = 0;                     // 活动状态开始时间


// MPU6050寄存器定义
#define MPU6050_REG_PWR_MGMT_1    0x6B    // 电源管理寄存器1
#define MPU6050_REG_PWR_MGMT_2    0x6C    // 电源管理寄存器2
#define MPU6050_REG_GYRO_CONFIG    0x1B    // 陀螺仪配置寄存器
#define MPU6050_REG_ACCEL_CONFIG   0x1C    // 加速度计配置寄存器
#define MPU6050_REG_ACCEL_XOUT_H   0x3B    // 加速度计X轴高字节
#define MPU6050_REG_SMPLRT_DIV     0x19    // 采样率分频器
#define MPU6050_REG_CONFIG         0x1A    // 配置寄存器

// 计算动作变化幅度
static uint32_t calculate_motion_magnitude(int16_t accel_x, int16_t accel_y, int16_t accel_z,
                                        int16_t gyro_x, int16_t gyro_y, int16_t gyro_z) {
    // 计算当前样本与历史样本的差值绝对值总和
    motion_sample_t *prev = &motion_history[(motion_index == 0) ? (MOTION_HISTORY_SIZE - 1) : (motion_index - 1)];
    
    uint32_t accel_diff = abs(accel_x - prev->accel_x) + 
                          abs(accel_y - prev->accel_y) + 
                          abs(accel_z - prev->accel_z);
                          
    uint32_t gyro_diff = 0;
    if (gyro_enabled) {
        gyro_diff = abs(gyro_x - prev->gyro_x) + 
                    abs(gyro_y - prev->gyro_y) + 
                    abs(gyro_z - prev->gyro_z);
    }
    
    return accel_diff + gyro_diff;
}

// 写MPU6050寄存器
static esp_err_t mpu6050_write_reg(uint8_t reg, uint8_t data) {
    uint8_t write_buf[2] = {reg, data};
    return i2c_master_write_to_device(MPU6050_I2C_PORT, MPU6050_I2C_ADDR,
                                     write_buf, sizeof(write_buf),
                                     pdMS_TO_TICKS(50));
}

// I2C初始化
static esp_err_t i2c_master_init(){
    i2c_config_t conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = MPU6050_SDA_GPIO,
        .scl_io_num = MPU6050_SCL_GPIO,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = 400000
    };
    
    ESP_ERROR_CHECK(i2c_param_config(MPU6050_I2C_PORT, &conf));
    return i2c_driver_install(MPU6050_I2C_PORT, conf.mode, 0, 0, 0);
}

esp_err_t mpu6050_init(void){
    esp_err_t ret;
    
    // 初始化I2C
    if((ret = i2c_master_init()) != ESP_OK){
        ESP_LOGE(TAG, "I2C init failed: 0x%x", ret);
        return ret;
    }
    
    // 重置所有寄存器
    if((ret = mpu6050_write_reg(MPU6050_REG_PWR_MGMT_1, 0x80)) != ESP_OK) {
        ESP_LOGE(TAG, "Reset failed: 0x%x", ret);
        return ret;
    }
    vTaskDelay(pdMS_TO_TICKS(100));  // 等待重置完成
    
    // 唤醒MPU6050
    if((ret = mpu6050_write_reg(MPU6050_REG_PWR_MGMT_1, 0x00)) != ESP_OK) {
        ESP_LOGE(TAG, "Wakeup failed: 0x%x", ret);
        return ret;
    }
    
    // 设置初始采样率为苏醒状态下的25Hz (8kHz/(1+319) = 25Hz)
    if((ret = mpu6050_write_reg(MPU6050_REG_SMPLRT_DIV, 319)) != ESP_OK) {
        ESP_LOGE(TAG, "Sample rate config failed: 0x%x", ret);
        return ret;
    }
    
    // 配置加速度计量程为±2g
    if((ret = mpu6050_write_reg(MPU6050_REG_ACCEL_CONFIG, 0x00)) != ESP_OK) {
        ESP_LOGE(TAG, "Accel config failed: 0x%x", ret);
        return ret;
    }
    
    // 配置陀螺仪量程为±250°/s
    if((ret = mpu6050_write_reg(MPU6050_REG_GYRO_CONFIG, 0x00)) != ESP_OK) {
        ESP_LOGE(TAG, "Gyro config failed: 0x%x", ret);
        return ret;
    }
    
    // 初始化历史记录数组
    memset(motion_history, 0, sizeof(motion_history));
    motion_index = 0;
    is_sleeping = false;
    gyro_enabled = true;
    stable_start_time = 0;
    active_start_time = 0;
    
    ESP_LOGI(TAG, "MPU6050 initialized with %dHz sampling rate (awake state)", MPU_AWAKE_SAMPLE_RATE_HZ);
    return ESP_OK;
}

void mpu6050_read_raw(int16_t *accel_x, int16_t *accel_y, int16_t *accel_z,
                     int16_t *gyro_x, int16_t *gyro_y, int16_t *gyro_z){
    uint8_t data[14];
    
    // 读取0x3B寄存器开始的14字节数据
    i2c_master_write_read_device(MPU6050_I2C_PORT, MPU6050_I2C_ADDR,
                                (uint8_t[]){0x3B}, 1, data, 14, pdMS_TO_TICKS(100));
    
    // 解析加速度计数据（小端模式）
    *accel_x = (data[0] << 8) | data[1];
    *accel_y = (data[2] << 8) | data[3];
    *accel_z = (data[4] << 8) | data[5];
    
    // 解析陀螺仪数据
    *gyro_x = (data[8] << 8) | data[9];
    *gyro_y = (data[10] << 8) | data[11];
    *gyro_z = (data[12] << 8) | data[13];
}

float mpu6050_read_temp(void){
    uint8_t data[2];
    i2c_master_write_read_device(MPU6050_I2C_PORT, MPU6050_I2C_ADDR,
                                (uint8_t[]){0x41}, 1, data, 2, pdMS_TO_TICKS(50));
    int16_t raw_temp = (data[0] << 8) | data[1];
    return (raw_temp / 340.0) + 36.53;
}

// 开启或关闭陀螺仪
esp_err_t mpu6050_enable_gyro(bool enable) {
    esp_err_t ret;
    uint8_t pwr_mgmt2_val = 0;
    
    if (enable == gyro_enabled) {
        // 已经是目标状态，无需改变
        return ESP_OK;
    }
    
    if (enable) {
        // 开启陀螺仪 (清除LP_WAKE_CTRL位并清除STBY_XG, STBY_YG, STBY_ZG位)
        pwr_mgmt2_val = 0x00;  // 全部启用
        ESP_LOGI(TAG, "Enabling gyroscope");
    } else {
        // 关闭陀螺仪 (设置STBY_XG, STBY_YG, STBY_ZG位)
        pwr_mgmt2_val = 0x07;  // 禁用陀螺仪XYZ轴，保持加速度计开启
        ESP_LOGI(TAG, "Disabling gyroscope to save power");
    }
    
    ret = mpu6050_write_reg(MPU6050_REG_PWR_MGMT_2, pwr_mgmt2_val);
    if (ret == ESP_OK) {
        gyro_enabled = enable;
    }
    
    return ret;
}

// 获取当前睡眠状态
bool mpu6050_is_sleeping(void) {
    return is_sleeping;
}

// 设置MPU6050采样率
esp_err_t mpu6050_set_sample_rate(uint8_t rate_hz) {
    // 计算需要的分频值: divider = 8000 / rate_hz - 1
    uint8_t divider = (8000 / rate_hz) - 1;
    esp_err_t ret = mpu6050_write_reg(MPU6050_REG_SMPLRT_DIV, divider);
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "MPU6050 sampling rate changed to %dHz (divider: %d)", rate_hz, divider);
    } else {
        ESP_LOGE(TAG, "Failed to set MPU6050 sampling rate: 0x%x", ret);
    }
    return ret;
}

// 更新运动状态并检测睡眠/唤醒
void mpu6050_update_motion_state(int16_t accel_x, int16_t accel_y, int16_t accel_z,
                              int16_t gyro_x, int16_t gyro_y, int16_t gyro_z) {
    // 更新运动历史记录
    motion_sample_t *current = &motion_history[motion_index];
    current->accel_x = accel_x;
    current->accel_y = accel_y;
    current->accel_z = accel_z;
    current->gyro_x = gyro_x;
    current->gyro_y = gyro_y;
    current->gyro_z = gyro_z;
    current->timestamp = (uint32_t)(xTaskGetTickCount() * portTICK_PERIOD_MS / 1000); // 转换为秒
    
    // 前进到下一个索引
    motion_index = (motion_index + 1) % MOTION_HISTORY_SIZE;
    
    // 只有在有足够的历史记录后才检测运动状态
    if (motion_index > 0) {
        // 计算动作变化幅度
        uint32_t motion_magnitude = calculate_motion_magnitude(
            accel_x, accel_y, accel_z, gyro_x, gyro_y, gyro_z);
        
        uint32_t current_time = (uint32_t)(xTaskGetTickCount() * portTICK_PERIOD_MS / 1000); // 当前时间(秒)
        
        // 睡眠状态检测逻辑
        if (!is_sleeping) {
            // 当前是醒着的状态，检查是否应该进入睡眠状态
            if (motion_magnitude < SLEEP_DETECT_THRESHOLD) {
                // 动作幅度小，可能是静止状态
                if (stable_start_time == 0) {
                    // 首次检测到静止状态，记录开始时间
                    stable_start_time = current_time;
                    ESP_LOGI(TAG, "Motion stable, monitoring for sleep state...");
                } else if (current_time - stable_start_time >= SLEEP_DETECT_TIME_MIN * 60) {
                    // 静止状态持续了足够长的时间，进入睡眠状态
                    is_sleeping = true;
                    stable_start_time = 0; // 重置计时器
                    
                    // 关闭陀螺仪以节省电能
                    mpu6050_enable_gyro(false);
                    
                    // 降低采样率为5Hz(睡眠状态)
                    mpu6050_set_sample_rate(MPU_SLEEP_SAMPLE_RATE_HZ);
                    
                    ESP_LOGI(TAG, "Sleep state detected, gyroscope disabled, sampling rate reduced to %dHz", MPU_SLEEP_SAMPLE_RATE_HZ);
                }
            } else {
                // 有明显动作，重置静止状态计时器
                stable_start_time = 0;
            }
        } else {
            // 当前是睡眠状态，检查是否应该唤醒
            if (motion_magnitude > SLEEP_DETECT_THRESHOLD * 2) { // 使用更高的阈值确认是真实活动
                // 检测到显著运动
                if (active_start_time == 0) {
                    // 首次检测到活动状态，记录开始时间
                    active_start_time = current_time;
                    ESP_LOGI(TAG, "Motion detected during sleep, monitoring for wake state...");
                } else if (current_time - active_start_time >= WAKE_DETECT_TIME_MIN * 60) {
                    // 活动状态持续了足够长的时间，退出睡眠状态
                    is_sleeping = false;
                    active_start_time = 0; // 重置计时器
                    
                    // 重新启用陀螺仪
                    mpu6050_enable_gyro(true);
                    
                    // 提高采样率为25Hz(苏醒状态)
                    mpu6050_set_sample_rate(MPU_AWAKE_SAMPLE_RATE_HZ);
                    
                    ESP_LOGI(TAG, "Wake state detected, gyroscope enabled, sampling rate increased to %dHz", MPU_AWAKE_SAMPLE_RATE_HZ);
                }
            } else {
                // 运动不显著，重置活动状态计时器
                active_start_time = 0;
            }
        }
    }
}
