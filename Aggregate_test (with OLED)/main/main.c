#include <stdio.h>
#include "esp_log.h"
#include "esp_system.h"
#include "nvs_flash.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "driver/gpio.h"
#include "driver/timer.h"

#include "adc_module.h"
#include "hr_calculation.h"
#include "pin_control.h"

static const char *TAG = "ECG_MAIN";

// 采样相关参数
#define ECG_SAMPLE_RATE_HZ    200     // 心电信号采样率 (Hz)
#define HR_PRINT_INTERVAL_MS     1000      // 心率打印间隔 (ms)

// 定时器配置
#define TIMER_GROUP            TIMER_GROUP_0
#define TIMER_ID               TIMER_0
#define TIMER_DIVIDER          80     // 80MHz / 80 = 1MHz
#define TIMER_SCALE            (TIMER_BASE_CLK / TIMER_DIVIDER)

// 任务句柄
TaskHandle_t sampling_task_handle = NULL;
TaskHandle_t hr_print_task_handle = NULL;

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
    const int PRINT_INTERVAL = 50;  // 每50个采样点打印一次，防止控制台刷新过快
    
    while (1) {
        // 等待定时器中断通知
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        
        // 读取AD8232心电信号
        ecg_value = adc_read_ecg_value();
        
        // 定期打印ECG值
        print_counter++;
        if (print_counter >= PRINT_INTERVAL) {
            // 打印原始ADC值和导联状态
            ESP_LOGI(TAG, "ECG Signal Value: %d, Leads Connected: %s", 
                     ecg_value, 
                     hr_check_leads_connected() ? "YES" : "NO");
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
        
        // 在串口打印心率和导联状态
        ESP_LOGI(TAG, "Heart Rate: %d BPM, Leads Connected: %s", 
                 heart_rate, leads_connected ? "YES" : "NO");
        
        // 每秒更新一次
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

// 初始化定时器
static void init_timer(void)
{
    // 计算定时器周期 (us)
    uint64_t timer_period_us = 1000000 / ECG_SAMPLE_RATE_HZ;
    
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
    ESP_LOGI(TAG, "ECG Heart Rate Monitor starting...");
    
    // 初始化NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
    
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
    
    // OLED显示器已移除
    // AD8232 SDN引脚控制已移除，保持悬空
    
    ESP_LOGI(TAG, "System Ready");
    
    // 创建采样任务
    xTaskCreate(ecg_sampling_task, "ecg_sampling", 2048, NULL, 5, &sampling_task_handle);
    
    // 创建心率打印任务
    xTaskCreate(hr_print_task, "hr_print", 2048, NULL, 4, &hr_print_task_handle);
    
    // 初始化定时器
    init_timer();
    
    ESP_LOGI(TAG, "System initialization complete");
}
