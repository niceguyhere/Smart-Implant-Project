#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "esp_adc/adc_oneshot.h"
#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"

// ADC通道定义
#define ADC_CHANNEL              ADC_CHANNEL_3  // GPIO3对应的ADC通道 - ESP32-C3上GPIO3通常对应ADC1_CH3
#define ADC_ATTEN               ADC_ATTEN_DB_12 // 0-3.3V输入范围
#define ADC_BIT_WIDTH           ADC_BITWIDTH_DEFAULT

// 初始化ADC模块
bool adc_init(void);

// 读取AD8232心电信号
int adc_read_ecg_value(void);

// 释放ADC资源
void adc_deinit(void);
