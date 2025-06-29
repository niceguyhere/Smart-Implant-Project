#include <stdio.h>
#include <string.h> // For strerror
#include <esp_log.h>
#include <esp_system.h>
#include <nvs_flash.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/queue.h>
#include <freertos/semphr.h>
#include <driver/gpio.h>
#include <driver/timer.h>
#include <time.h>
#include <sys/time.h> // 用于 settimeofday() 函数
#include <sys/stat.h> // 用于 stat() 函数
#include <unistd.h> // 用于 access() 函数
#include <inttypes.h> // 用于 PRIu32 等格式化宏
#include <errno.h> // 用于获取错误代码

#include "adc_module.h"
#include "config.h"
#include "hr_calculation.h"
#include "mpu6050.h"
#include "pin_control.h"
#include "battery_monitor.h"
#include "ntc_sensor.h"
#include "sd_card.h"
#include "ble_module.h" // 新增BLE模块头文件

static const char *TAG = "ECG_MAIN";

// 定时器配置
#define TIMER_GROUP            TIMER_GROUP_0
#define TIMER_ID               TIMER_0
#define TIMER_DIVIDER          80     // 80MHz / 80 = 1MHz
#define TIMER_SCALE            (TIMER_BASE_CLK / TIMER_DIVIDER)

// 任务句柄
TaskHandle_t sampling_task_handle = NULL;
TaskHandle_t hr_print_task_handle = NULL;
// Note: Task handles for MPU, Temp, Battery, Electrode, and Data Writer tasks will be declared later if needed,
// or they might be static within their respective modules if not controlled externally after creation.

// Sensor data structures (ECGData, LeadData, etc.) and DoubleBuffer are now defined in sd_card.h
// Global buffer variables (ecg_buffer, lead_buffer, etc.) are now static within sd_card.c
// Buffer management functions (init_double_buffer, add_..._data, swap_buffer) are now in sd_card.c
// File writing functions (write_..._data, create_filename) and data_writer_task are now in sd_card.c


// All SD card related functions (add_batt_data, swap_buffer, create_filename, write_..._data, data_writer_task)
// have been moved to sd_card.c and sd_card.h


// 定时器中断服务函数
static bool IRAM_ATTR timer_isr_callback(void *args)
{
    // 通知采样任务执行采样
    BaseType_t high_task_wakeup = pdFALSE;
    vTaskNotifyGiveFromISR(sampling_task_handle, &high_task_wakeup);
    return high_task_wakeup == pdTRUE; // 如果唤醒了高优先级任务，返回true
}

// 心电信号采样任务
static void ecg_sampling_task(void *arg)
{
    ESP_LOGI(TAG, "ECG sampling task started");
    
    int ecg_value = 0;
    int print_counter = 0;  // 用于控制打印频率
    // 计算打印间隔: 500Hz采样率下，如果要每秒打印一次，需要每500个点打印一次
    const int PRINT_INTERVAL = ECG_SAMPLE_RATE_HZ;  // 每秒打印一次
    
    while (1) {
        // 等待定时器中断通知
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        
        // 读取AD8232心电信号
        ecg_value = adc_read_ecg_value();
        
        // 添加到缓冲区
        sd_add_ecg_data(ecg_value);
        
        // 定期打印ECG值
        print_counter++;
        if (print_counter >= PRINT_INTERVAL) {
            // 只打印原始的ADC值
            ESP_LOGI(TAG, "ECG Signal Value: %d", ecg_value);
            print_counter = 0;  // 重置计数器
        }
        
        // 处理心电信号并计算心率
        hr_process_sample(ecg_value);
    }
}

// 心率显示任务 - 只显示在串口上
static void hr_print_task(void *arg)
{
    ESP_LOGI(TAG, "Heart rate print task started");
    
    uint8_t heart_rate = 0;
    bool leads_connected = false;
    
    while (1) {
        // 检查导联连接状态
        leads_connected = hr_check_leads_connected();
        
        // 获取当前心率值
        heart_rate = hr_get_rate();
        
        // 只打印心率，不再打印导联状态
        ESP_LOGI(TAG, "Heart Rate: %d BPM", heart_rate);
        
        // 每秒更新一次
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

// 运动数据采集任务
static void motion_sampling_task(void *arg) {
    int16_t accel_x, accel_y, accel_z;
    int16_t gyro_x, gyro_y, gyro_z;
    bool last_sleep_state = false;
    
    ESP_LOGI(TAG, "Motion sampling task started (Awake: %dHz, Sleep: %dHz)", MPU_AWAKE_SAMPLE_RATE_HZ, MPU_SLEEP_SAMPLE_RATE_HZ);
    
    while (1) {
        // 读取传感器数据
        mpu6050_read_raw(&accel_x, &accel_y, &accel_z, &gyro_x, &gyro_y, &gyro_z);
        
        // 添加到缓冲区
        sd_add_mpu_data(accel_x, accel_y, accel_z, gyro_x, gyro_y, gyro_z);
        
        // 更新运动状态并检测睡眠
        mpu6050_update_motion_state(accel_x, accel_y, accel_z, gyro_x, gyro_y, gyro_z);
        
        // 检查睡眠状态是否发生变化
        bool current_sleep_state = mpu6050_is_sleeping();
        if (current_sleep_state != last_sleep_state) {
            if (current_sleep_state) {
                ESP_LOGI(TAG, "患者进入睡眠状态，陀螺仪已关闭以节省电量");
            } else {
                ESP_LOGI(TAG, "患者已苏醒，陀螺仪已重新启用");
            }
            last_sleep_state = current_sleep_state;
        }
        
        // 计算打印间隔: 当患者苏醒时是%d秒打印一次，睡眠时是%d秒打印一次
        int print_interval = current_sleep_state ? MPU_PRINT_INTERVAL_MS * 2 : MPU_PRINT_INTERVAL_MS;
        static int log_counter = 0;
        log_counter++;
        
        if ((log_counter * (current_sleep_state ? 1000/MPU_SLEEP_SAMPLE_RATE_HZ : 1000/MPU_AWAKE_SAMPLE_RATE_HZ)) >= print_interval) {
            ESP_LOGI(TAG, "Accel: X:%6d Y:%6d Z:%6d  Gyro: X:%6d Y:%6d Z:%6d  %s",
                     accel_x, accel_y, accel_z, gyro_x, gyro_y, gyro_z,
                     current_sleep_state ? "[SLEEP]" : "");
            log_counter = 0;
        }
        
        // 根据睡眠状态动态调整采样速率
        int delay_ms = current_sleep_state ? 1000/MPU_SLEEP_SAMPLE_RATE_HZ : 1000/MPU_AWAKE_SAMPLE_RATE_HZ;
        vTaskDelay(pdMS_TO_TICKS(delay_ms)); // 苏醒时%dHz，睡眠时%dHz
    }
}

// 温度打印任务 - 5Hz采样率
static void temp_print_task(void *arg) {
    ESP_LOGI(TAG, "Temperature monitoring task started at %dHz", 1000/MPU_TEMP_INTERVAL_MS);
    
    // 初始化NTC温度值
    float ntc1_temp = 0.0f;
    float ntc2_temp = 0.0f;
    bool ntc_initialized = false; // Flag to track NTC initialization

    // Initialize NTC sensor once at the start of the task
    if (ntc_sensor_init()) {
        ntc_initialized = true;
        ESP_LOGI(TAG, "NTC sensor initialized for temp_print_task.");
    } else {
        ESP_LOGE(TAG, "NTC sensor failed to initialize for temp_print_task. NTC readings will be zero.");
    }
    
    while (1) {
        // 读取MPU6050内部温度
        float mpu_temp = mpu6050_read_temp();
        
        // 尝试获取NTC温度传感器的值
        if (ntc_initialized) { // Only read if NTC was initialized successfully
            ntc1_temp = ntc_read_temp1();
            ntc2_temp = ntc_read_temp2();
        } else {
            // Keep NTC temps at 0.0f or some other default if init failed
            ntc1_temp = 0.0f; 
            ntc2_temp = 0.0f;
        }
        
        // 添加到缓冲区
        sd_add_temp_data(mpu_temp, ntc1_temp, ntc2_temp);
        
        // 打印温度值
        ESP_LOGI(TAG, "Temperatures - MPU: %.2f°C, NTC1: %.2f°C, NTC2: %.2f°C", 
                 mpu_temp, ntc1_temp, ntc2_temp);
        
        vTaskDelay(pdMS_TO_TICKS(MPU_TEMP_INTERVAL_MS));  // 5Hz采样率
    }
}

// SD卡初始化任务 - 如果成功则退出任务
// sd_card_init_task removed, initialization handled by sd_card_init() and sd_start_background_tasks() in app_main.

// 电池监测任务 - 0.2Hz采样率 (5秒一次)
static void battery_task(void *arg) {
    battery_init();
    ESP_LOGI(TAG, "电池监测任务启动, 采样率: %.1fHz, 分压电阻: %.1fkΩ:%.1fkΩ", 
        1000.0f/BATT_SAMPLE_INTERVAL, BATT_DIV_R1, BATT_DIV_R2);
    
    while(1) {
        float voltage = read_filtered_voltage();
        int soc = estimate_soc(voltage);
        
        // 添加到缓冲区
        sd_add_batt_data(voltage, soc);
        
        // 日志输出电池状态
        ESP_LOGI(TAG, "电池状态: %.2fV (%d%%)", voltage, soc);
        
        // 使用配置定义的低电报警阈值
        if(voltage < BATT_LOW_THRESHOLD) {
            ESP_LOGW(TAG, "电量不足, 请及时充电! (%.1fV)", voltage);
        }
        
        // 使用配置文件中定义的采样间隔
        vTaskDelay(pdMS_TO_TICKS(BATT_SAMPLE_INTERVAL)); // 定时更新
    }
}

// 初始化定时器
static void init_timer(void)
{
    // 计算定时器周期 (us) - 设置心电信号采样率
    uint64_t timer_period_us = 1000000 / ECG_SAMPLE_RATE_HZ;
    
    ESP_LOGI(TAG, "设置心电采样定时器为%dHz采样率", ECG_SAMPLE_RATE_HZ);
    
    // 定时器配置
    timer_config_t config = {
        .divider = TIMER_DIVIDER,
        .counter_dir = TIMER_COUNT_UP,
        .counter_en = TIMER_PAUSE,
        .alarm_en = TIMER_ALARM_EN,
        .auto_reload = TIMER_AUTORELOAD_EN,
    };
    timer_init(TIMER_GROUP, TIMER_ID, &config);
    
    // 设置定时器计数初值
    timer_set_counter_value(TIMER_GROUP, TIMER_ID, 0);
    
    // 设置定时器告警值
    timer_set_alarm_value(TIMER_GROUP, TIMER_ID, timer_period_us);
    
    // 注册中断服务函数
    timer_enable_intr(TIMER_GROUP, TIMER_ID);
    timer_isr_callback_add(TIMER_GROUP, TIMER_ID, timer_isr_callback, NULL, 0);
    
    // 启动定时器
    timer_start(TIMER_GROUP, TIMER_ID);
    
    ESP_LOGI(TAG, "Timer initialized with period: %llu us", timer_period_us);
}

void app_main(void)
{
    ESP_LOGI(TAG, "*** Heart Rate Monitor System Starting ***");
    ESP_LOGI(TAG, "Free memory: %lu bytes", esp_get_free_heap_size());
    
    // 初始化系统时间，防止使用1970年的默认时间
    // 注意：实际项目应使用RTC或NTP进行时间同步
    struct tm timeinfo = {
        .tm_year = 2025 - 1900,  // 年份需要减1900
        .tm_mon = 5 - 1,         // 月份从0开始，所以减1
        .tm_mday = 30,           // 日期
        .tm_hour = 8,           // 小时
        .tm_min = 0,            // 分钟
        .tm_sec = 0             // 秒
    };
    time_t t = mktime(&timeinfo);
    struct timeval now = { .tv_sec = t };
    settimeofday(&now, NULL);
    ESP_LOGI(TAG, "System time set to 2025-05-30 08:00:00");
    
    ESP_LOGI(TAG, "ECG Heart Rate Monitor starting...");
    
    // 初始化NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    // 初始化BLE模块 (需要在NVS之后)
    ble_module_init();
    
    // 初始化各个模块
    if (!pin_control_init()) {
        ESP_LOGE(TAG, "Failed to initialize pin control");
        return;
    }
    
    if (!adc_init()) {
        ESP_LOGE(TAG, "Failed to initialize ADC");
        return;
    }
    
    if (!hr_init()) {
        ESP_LOGE(TAG, "Failed to initialize heart rate calculation");
        return;
    }
    
    // 初始化MPU6050
    if (mpu6050_init() != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize MPU6050");
        return;
    }
    
    // OLED显示器已移除
    // AD8232 SDN引脚控制已移除，保持悬空
    
    ESP_LOGI(TAG, "System Ready");
    
    // Initialize SD card and related services
    ESP_LOGI(TAG, "Initializing SD card...");
    if (sd_card_init(true)) { // Attempt to format if mount fails
        ESP_LOGI(TAG, "SD card initialized successfully.");
        sd_buffers_init();
        ESP_LOGI(TAG, "SD card data buffers initialized.");
        sd_start_background_tasks();
        ESP_LOGI(TAG, "SD card background data writer task started.");
    } else {
        ESP_LOGE(TAG, "Failed to initialize SD card. Data logging will be unavailable.");
    }

    // 创建采样任务
    xTaskCreate(ecg_sampling_task, "ecg_sampling", 4096, NULL, 5, &sampling_task_handle);
    
    // 创建心率打印任务
    xTaskCreate(hr_print_task, "hr_print", 2048, NULL, 4, &hr_print_task_handle);
    
    // 创建运动数据采集任务
    xTaskCreate(motion_sampling_task, "motion_sampling", 2560, NULL, 3, NULL);
    
    // 创建温度打印任务
    xTaskCreate(temp_print_task, "temp_print", 2348, NULL, 2, NULL);
    
    // 创建电池监测任务
    xTaskCreate(battery_task, "battery", 2048, NULL, 2, NULL);
    
    // 创建新的电极状态监测任务 (从pin_control模块)
    xTaskCreate(electrode_status_task, "electrode_status_task", 2048, NULL, 2, NULL);
    ESP_LOGI(TAG, "Electrode status monitoring task (from pin_control) started at 1Hz");
    
    // Removed redundant NTC initialization and ntc_temp_monitor_task creation here.
    // NTC initialization and reading are now handled within temp_print_task.
    
    // 初始化定时器
    init_timer();
    
    ESP_LOGI(TAG, "System initialization complete");
}
