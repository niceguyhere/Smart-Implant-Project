#pragma once

#include <stdbool.h>
#include <stdint.h> // For uint32_t, int16_t, etc.
#include <sys/time.h> // For struct timeval
#include "esp_err.h"
#include "freertos/FreeRTOS.h" // For SemaphoreHandle_t
#include "freertos/semphr.h"   // For SemaphoreHandle_t

// SD卡SPI引脚定义
#define SD_PIN_MISO  5   // GPIO5
#define SD_PIN_MOSI  6   // GPIO6
#define SD_PIN_CLK   4   // GPIO4
#define SD_PIN_CS    7   // GPIO7

#define SD_MOUNT_POINT "/sdcard"  // 挂载点

// 传感器数据结构定义 (moved from main.c)
#pragma pack(push, 1)  // 1字节对齐，节省空间

// 心电数据结构
typedef struct {
    struct timeval timestamp;  // System time timestamp (seconds and microseconds)
    int16_t value;       // 心电ADC值
} ECGData_t;

// 导联状态数据结构
typedef struct {
    struct timeval timestamp;  // System time timestamp (seconds and microseconds)
    uint8_t status;      // 0=连接, 1=断开
} LeadData_t;

// MPU6050数据结构
typedef struct {
    struct timeval timestamp;  // System time timestamp (seconds and microseconds)
    int16_t accel_x;     // 加速度X
    int16_t accel_y;     // 加速度Y
    int16_t accel_z;     // 加速度Z
    int16_t gyro_x;      // 陀螺仪X
    int16_t gyro_y;      // 陀螺仪Y
    int16_t gyro_z;      // 陀螺仪Z
} MPUData_t;

// 温度数据结构
typedef struct {
    struct timeval timestamp;  // System time timestamp (seconds and microseconds)
    int16_t mpu_temp;    // MPU6050内部温度 (放大100倍)
    int16_t ntc1_temp;   // NTC1温度 (放大100倍)
    int16_t ntc2_temp;   // NTC2温度 (放大100倍)
} TempData_t;

// 电池数据结构
typedef struct {
    struct timeval timestamp;  // System time timestamp (seconds and microseconds)
    uint16_t voltage;    // 电压值 (mV)
    uint8_t soc;         // 电量百分比
} BattData_t;

#pragma pack(pop)

// 双缓冲结构定义 (moved from main.c)
typedef struct {
    void *buffer1;
    void *buffer2;
    void *active_buf;    // 当前活动缓冲区
    void *ready_buf;     // 准备写入的缓冲区
    uint16_t count;      // 当前数据计数
    uint16_t capacity;   // 缓冲区容量
    SemaphoreHandle_t mutex;
} DoubleBuffer_t;

// --- Existing sd_card.h functions ---
/**
 * @brief 初始化SD卡并挂载文件系统
 * 
 * @param format_if_failed 如果挂载失败是否格式化
 * @return true 初始化成功
 * @return false 初始化失败
 */
bool sd_card_init(bool format_if_failed);

/**
 * @brief 写入文件到SD卡
 * 
 * @param filename 文件名 (包括路径)
 * @param data 要写入的数据
 * @return esp_err_t ESP_OK 成功，其他值表示失败
 */
esp_err_t sd_card_write_file(const char *filename, const char *data);

/**
 * @brief 读取SD卡上的文件
 * 
 * @param filename 文件名 (包括路径)
 * @param data 数据缓冲区
 * @param max_size 缓冲区最大大小
 * @return esp_err_t ESP_OK 成功，其他值表示失败
 */
esp_err_t sd_card_read_file(const char *filename, char *data, size_t max_size);

/**
 * @brief 递归创建目录
 * 
 * @param path 要创建的目录路径
 * @return esp_err_t ESP_OK 成功，其他值表示失败
 */
esp_err_t sd_mkdirs(const char *path);

/**
 * @brief 检查SD卡是否已挂载
 * 
 * @return true SD卡已挂载
 * @return false SD卡未挂载
 */
bool sd_card_mounted(void);

/**
 * @brief 卸载SD卡
 */
void sd_card_deinit(void);

// --- New structures and functions for BLE data retrieval ---

// Structure to hold various sensor raw data for BLE transmission
// These are typically the latest values read from the respective sensor buffers.
#pragma pack(push, 1)
typedef struct {
    uint16_t battery_voltage_mv; // Battery voltage in millivolts
    int16_t  ntc1_temp_scaled;   // NTC1 temperature (Celsius * 100)
    int16_t  ntc2_temp_scaled;   // NTC2 temperature (Celsius * 100)
    int16_t  mpu_temp_scaled;    // MPU6050 internal temperature (Celsius * 100)
    uint8_t  electrode_status;   // Electrode status (0 = connected, 1 = disconnected)
    uint8_t  battery_soc;        // Battery state of charge (0-100%)
    int16_t  accel_x;            // MPU6050 accelerometer X-axis raw value
    int16_t  accel_y;            // MPU6050 accelerometer Y-axis raw value
    int16_t  accel_z;            // MPU6050 accelerometer Z-axis raw value
    int16_t  gyro_x;             // MPU6050 gyroscope X-axis raw value
    int16_t  gyro_y;             // MPU6050 gyroscope Y-axis raw value
    int16_t  gyro_z;             // MPU6050 gyroscope Z-axis raw value
} OtherSensorsRawDataForBLE_t;
#pragma pack(pop)

/**
 * @brief Retrieves the latest raw ECG sample from the SD card buffer.
 * 
 * @return int16_t The latest raw ECG ADC value. Returns 0 or a specific error/default 
 *                 value if the buffer is empty or an error occurs.
 */
int16_t sd_get_latest_ecg_raw_sample(void);

/**
 * @brief Retrieves the latest set of other raw sensor data from the SD card buffers.
 * 
 * @param target_struct Pointer to an OtherSensorsRawDataForBLE_t structure to be filled.
 * @return true If data was successfully retrieved and the structure is filled.
 * @return false If data could not be retrieved (e.g., buffers empty, error).
 */
bool sd_get_latest_other_sensors_raw_data(OtherSensorsRawDataForBLE_t *target_struct);

// --- End of new structures and functions ---

// --- New public functions for the module ---

/**
 * @brief 初始化所有数据缓冲区
 */
void sd_buffers_init(void);

/**
 * @brief 添加心电数据到SD卡缓冲区
 * @param value 心电ADC值
 */
void sd_add_ecg_data(int16_t value);

/**
 * @brief 添加导联状态数据到SD卡缓冲区
 * @param status 导联状态 (0=连接, 1=断开)
 */
void sd_add_lead_data(uint8_t status);

/**
 * @brief 添加MPU6050数据到SD卡缓冲区
 */
void sd_add_mpu_data(int16_t accel_x, int16_t accel_y, int16_t accel_z,
                     int16_t gyro_x, int16_t gyro_y, int16_t gyro_z);

/**
 * @brief 添加温度数据到SD卡缓冲区
 * @param mpu_temp_scaled MPU6050内部温度 (实际值 * 100)
 * @param ntc1_temp_scaled NTC1温度 (实际值 * 100)
 * @param ntc2_temp_scaled NTC2温度 (实际值 * 100)
 */
void sd_add_temp_data(int16_t mpu_temp_scaled, int16_t ntc1_temp_scaled, int16_t ntc2_temp_scaled);

/**
 * @brief 添加电池数据到SD卡缓冲区
 * @param voltage_mv 电压值 (mV)
 * @param soc_percent 电量百分比
 */
void sd_add_batt_data(uint16_t voltage_mv, uint8_t soc_percent);

/**
 * @brief 启动SD卡后台任务 (数据写入和初始化)
 */
void sd_start_background_tasks(void);

