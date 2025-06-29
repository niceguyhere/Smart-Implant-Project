#include "battery_monitor.h"
#include "config.h"
#include "adc_module.h"  // 包含共享ADC模块
#include "esp_adc/adc_oneshot.h"
// #include "esp_adc/adc_cali.h" // adc_cali_handle is now in adc_module.h as extern, or managed internally
#include "esp_adc/adc_cali_scheme.h"
#include <stdio.h>
#include <math.h>
#include "esp_log.h"

static const char *TAG = "BATTERY";

// 使用共享的ADC句柄
static adc_oneshot_unit_handle_t adc1_handle = NULL;

// ADC校准句柄 - Specific for battery channel if different attenuation/config from ECG
static adc_cali_handle_t batt_adc_cali_handle = NULL; // Renamed for clarity

// 移动平均滤波器
#define FILTER_WINDOW 8
static float voltage_buffer[FILTER_WINDOW] = {0};
static int buffer_index = 0;

void battery_init() {
    // 获取已初始化的ADC句柄
    adc1_handle = adc_get_handle();
    if (adc1_handle == NULL) {
        ESP_LOGE(TAG, "Failed to get ADC handle, make sure adc_init() was called");
        return;
    }
    
    // 为电池检测配置额外的ADC通道(GPIO1)
    if (!adc_configure_extra_channel(BATT_ADC_CHANNEL, BATT_ADC_ATTEN)) {
        ESP_LOGE(TAG, "Failed to configure battery ADC channel");
        return;
    }
    
    // 初始化ADC校准
    adc_cali_curve_fitting_config_t cali_config = {
        .unit_id = ADC_UNIT_1,
        .atten = BATT_ADC_ATTEN,
        .bitwidth = ADC_BITWIDTH_12,
    };
    ESP_ERROR_CHECK(adc_cali_create_scheme_curve_fitting(&cali_config, &batt_adc_cali_handle));
    
    // 配置ADC通道
    adc_oneshot_chan_cfg_t channel_config = {
        .atten = BATT_ADC_ATTEN,
        .bitwidth = ADC_BITWIDTH_12,
    };
    ESP_ERROR_CHECK(adc_oneshot_config_channel(adc1_handle, BATT_ADC_CHANNEL, &channel_config));
}

float read_filtered_voltage() {
    int raw_adc_value;
    bool adc_read_successful = adc_read_raw_protected(BATT_ADC_CHANNEL, &raw_adc_value);

    if (adc_read_successful) {
        // ADC read successful, calculate voltage and update filter buffer
        int voltage_mv = 0;
        if (batt_adc_cali_handle != NULL && adc_cali_raw_to_voltage(batt_adc_cali_handle, raw_adc_value, &voltage_mv) == ESP_OK) {
            // Calibration successful
        } else {
            ESP_LOGW(TAG, "Battery ADC calibration failed or not available, using raw value approximation for raw_adc_value %d.", raw_adc_value);
            voltage_mv = (int)(raw_adc_value * 3300.0f / 4095.0f); // Fallback, ensure Vref is appropriate
        }
        float vout = voltage_mv / 1000.0f;
        float calculated_voltage_this_sample = vout * (BATT_DIV_R1 + BATT_DIV_R2) / BATT_DIV_R2;
        
        voltage_buffer[buffer_index] = calculated_voltage_this_sample;
    } else {
        // ADC read failed. Log a warning.
        // voltage_buffer[buffer_index] will retain its old value from the previous cycle.
        ESP_LOGW(TAG, "Protected ADC read for battery channel failed. Filter at index %d will use stale data.", buffer_index);
    }
    
    // 移动平均滤波 (continues after the if/else block)
    buffer_index = (buffer_index + 1) % FILTER_WINDOW;
    
    float sum = 0;
    for(int i=0; i<FILTER_WINDOW; i++) {
        sum += voltage_buffer[i];
    }
    return sum / FILTER_WINDOW;
}

int estimate_soc(float voltage) {
    // 基于电压的简单SOC估算
    const float volt_points[] = {3.3, 3.7, 3.9, 4.2};
    const int soc_points[] = {0, 20, 80, 100};
    
    if(voltage <= volt_points[0]) return 0;
    if(voltage >= volt_points[3]) return 100;
    
    for(int i=1; i<4; i++) {
        if(voltage < volt_points[i]) {
            float ratio = (voltage - volt_points[i-1]) / (volt_points[i] - volt_points[i-1]);
            return soc_points[i-1] + ratio*(soc_points[i]-soc_points[i-1]);
        }
    }
    return 0;
}
