#pragma once

// 传感器采样率配置
#define ECG_SAMPLE_RATE_HZ     500   // 心电信号采样率(Hz)
#define ECG_PRINT_INTERVAL_MS  1000  // 心电打印间隔(ms)

// 导联状态检测采样率
#define ELECTRODE_CHECK_INTERVAL_MS 1000  // 导联状态检测采样间隔(ms)

// MPU6050采样率配置
#define MPU_AWAKE_SAMPLE_RATE_HZ 25     // 患者苏醒时MPU采样率(Hz)
#define MPU_SLEEP_SAMPLE_RATE_HZ 5      // 患者睡眠时MPU采样率(Hz)
#define MPU_PRINT_INTERVAL_MS   200    // MPU数据打印间隔(ms)

// 温度采样率配置
#define MPU_TEMP_INTERVAL_MS    200    // MPU内部温度采样间隔(ms)
#define NTC_TEMP_INTERVAL_MS    200    // NTC温度采样间隔(ms)


// 电池监测配置
#define BATT_ADC_CHANNEL     ADC_CHANNEL_1  // GPIO1对应ADC1通道1
#define BATT_ADC_ATTEN       ADC_ATTEN_DB_6  // 2.2V量程，适合1.31V输入
#define BATT_DIV_R1          22.0f          // 上端电阻22kΩ
#define BATT_DIV_R2          10.0f          // 下端电阻10kΩ
#define BATT_FULL_VOLTAGE    4.2f           // 满电电压
#define BATT_EMPTY_VOLTAGE   3.3f           // 绝对低电电压
#define BATT_LOW_THRESHOLD   3.5f           // 低电报警阈值
#define BATT_SAMPLE_INTERVAL 5000           // 采样间隔(ms) - 0.2Hz
#define BATT_SAMPLE_COUNT    64             // 过采样次数

// NTC温度传感器配置
#define NTC_ADC_CHANNEL_1      ADC_CHANNEL_0  // GPIO0
#define NTC_ADC_CHANNEL_2      ADC_CHANNEL_2  // GPIO2
#define NTC_ADC_ATTEN          ADC_ATTEN_DB_12 // 0-3.3V量程
#define NTC_B_VALUE            3435
#define NTC_NOMINAL_RESISTANCE 10000  // 25°C时为10kΩ
#define NTC_SERIES_RESISTOR    10000  // 分压电阻10kΩ
#define NTC_NOMINAL_TEMP       25.0f  // 标称温度25°C
#define NTC_SAMPLES_COUNT      10     // 软件滤波采样次数

// ADC Module Specific Configurations
#define ADC_MUTEX_TIMEOUT_MS         50     // Timeout for acquiring ADC mutex (ms)
#define ADC_ERROR_LOG_INTERVAL_MS    5000   // Interval for logging ADC read errors (ms)
#define ADC_CHANNEL_GPIO_NUM         3      // Default ECG ADC Channel GPIO (e.g., GPIO3 for ADC1_CH3)
