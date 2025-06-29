#include "ntc_sensor.h"
#include "adc_module.h"
#include "config.h"       // 包含采样率和传感器配置
#include <math.h>
#include <string.h>
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "adc_module.h" // For adc_read_raw_protected and adc_get_handle

static const char *TAG = "NTC";

// ADCu53e5u67c4

static adc_oneshot_unit_handle_t adc1_handle = NULL;

// u6ee4u6ce2u7f13u51b2u533a
static int ntc1_samples[NTC_SAMPLES_COUNT] = {0};
static int ntc2_samples[NTC_SAMPLES_COUNT] = {0};
static int sample_index = 0;

/**
 * @brief u8ba1u7b97NTCu7684u6e29u5ea6
 * @param adc_value ADCu6570u503c
 * @return u6e29u5ea6(u6444u6c0fu5ea6)
 */
static float calculate_temperature(int adc_value) {
    // u8f6cu6362u4e3au7535u538b(u6bebu4f0f)
    int voltage_mv = 0;
    if (adc_cali_raw_to_voltage(adc1_cali_handle, adc_value, &voltage_mv) != ESP_OK) {
        ESP_LOGE(TAG, "ADC calibration failed");
        return 0.0f;
    }
    
    // u7535u538bu5206u538bu8ba1u7b97uff08V = mV / 1000uff09
    float voltage = voltage_mv / 1000.0f;
    
    // u8ba1u7b97NTCu963bu503c (R_ntc = R_series * (V_ref - V_out) / V_out)
    float ntc_resistance = NTC_SERIES_RESISTOR * voltage / (3.3f - voltage);
    
    // u4f7fu7528Bu503cu516cu5f0fu8ba1u7b97u6e29u5ea6
    // 1/T = 1/T0 + (1/B) * ln(R/R0)
    float temp_kelvin = 1.0f / ((1.0f / (NTC_NOMINAL_TEMP + 273.15f)) + 
                              (1.0f / NTC_B_VALUE) * log(ntc_resistance / NTC_NOMINAL_RESISTANCE));
    
    // u8f6cu6362u4e3au6444u6c0fu5ea6
    return temp_kelvin - 273.15f;
}

/**
 * @brief u8bfbu53d6ADCu5e76u8fdbu884cu6ee4u6ce2
 * @param channel ADCu901au9053
 * @param samples u6837u672cu7f13u51b2u533a
 * @return u6ee4u6ce2u540eu7684ADCu503c
 */
static int read_adc_filtered(adc_channel_t channel, int *samples) {
    // u8bfbu53d6u5f53u524dADCu503c
    int temp_adc_raw; // Use a temporary variable for the new reading
    if (adc_read_raw_protected(channel, &temp_adc_raw)) {
        // ADC read successful, update the corresponding slot in the samples buffer
        samples[sample_index] = temp_adc_raw;
    } else {
        // ADC read failed. Log a warning.
        // samples[sample_index] will retain its old value from the previous cycle.
        // The filter will operate with one stale data point, which is generally acceptable.
        ESP_LOGW(TAG, "Protected ADC read for NTC channel %d failed. Filter at index %d will use stale data.", channel, sample_index);
    }
    // The rest of the logic (summation and averaging) proceeds using the samples buffer.
    
    // u8ba1u7b97u5e73u5747u503c
    int sum = 0;
    for (int i = 0; i < NTC_SAMPLES_COUNT; i++) {
        sum += samples[i];
    }
    
    return sum / NTC_SAMPLES_COUNT;
}

bool ntc_sensor_init(void) {
    // u83b7u53d6ADCu53e5u67c4
    adc1_handle = adc_get_handle();
    if (adc1_handle == NULL) {
        ESP_LOGE(TAG, "Failed to get ADC handle");
        return false;
    }
    
    // u914du7f6eu4e24u4e2aNTCu4f20u611fu5668u7684ADCu901au9053
    if (!adc_configure_extra_channel(NTC_ADC_CHANNEL_1, NTC_ADC_ATTEN)) {
        if (!adc_configure_extra_channel(NTC_ADC_CHANNEL_2, NTC_ADC_ATTEN)) {
            ESP_LOGE(TAG, "Failed to configure NTC2 ADC channel");
            return false;
        }
    }
    
    // u521du59cbu5316u6ee4u6ce2u5668u7f13u51b2u533a
    memset(ntc1_samples, 0, sizeof(ntc1_samples));
    memset(ntc2_samples, 0, sizeof(ntc2_samples));
    sample_index = 0;
    
    ESP_LOGI(TAG, "NTC sensor initialized successfully");
    return true;
}

float ntc_read_temp1(void) {
    int adc_filtered = read_adc_filtered(NTC_ADC_CHANNEL_1, ntc1_samples);
    return calculate_temperature(adc_filtered);
}

float ntc_read_temp2(void) {
    int adc_filtered = read_adc_filtered(NTC_ADC_CHANNEL_2, ntc2_samples);
    return calculate_temperature(adc_filtered);
}

void ntc_temp_monitor_task(void *pvParameters) {
    // u521du59cbu65e5u5b9au4e8cu540eu7684u6e29u5ea6u4f20u611fu5668u7684u91c7u6837u7387
    ESP_LOGI(TAG, "NTC temperature monitoring task started at %dHz", 1000/NTC_TEMP_INTERVAL_MS);
    
    // u5faau73afu8bfbu53d6u548cu6253u5370u6e29u5ea6
    while (1) {
        // u66f4u65b0u91c7u6837u7d22u5f15
        sample_index = (sample_index + 1) % NTC_SAMPLES_COUNT;
        
        // u8bfbu53d6u4e24u4e2au4f20u611fu5668u7684u6e29u5ea6
        float temp1 = ntc_read_temp1();
        float temp2 = ntc_read_temp2();
        
        // u6253u5370u6e29u5ea6u503c
        ESP_LOGI(TAG, "NTC1: %.2f°C, NTC2: %.2f°C", temp1, temp2);
        
        // u4f7fu7528u65b0u7684u91c7u6837u7387 (5Hz = 200ms)
        vTaskDelay(pdMS_TO_TICKS(NTC_TEMP_INTERVAL_MS));
    }
}
