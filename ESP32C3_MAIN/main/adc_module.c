#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "esp_log.h"
// #include "driver/adc.h" // adc_oneshot API is preferred
// #include "esp_adc_cal.h" // adc_cali API is preferred
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h" // Include for mutex
#include "adc_module.h"
#include "config.h"

static const char *TAG = "ADC_MODULE";

// ADC handler
static adc_oneshot_unit_handle_t adc1_handle = NULL;
// Calibration handler for ECG channel
adc_cali_handle_t adc1_cali_handle = NULL; // Made extern in .h
static bool do_calibration_ecg = false; // Specific for ECG channel

// Mutex for protecting ADC operations
static SemaphoreHandle_t adc_mutex = NULL;

// 保存上一次有效的ADC读数 (for ECG)
static int last_valid_ecg_adc = 0;

// Initialize ADC module
bool adc_init(void) {
    // Create Mutex first
    adc_mutex = xSemaphoreCreateMutex();
    if (adc_mutex == NULL) {
        ESP_LOGE(TAG, "Failed to create ADC mutex");
        return false;
    }

    // ADC1 init
    adc_oneshot_unit_init_cfg_t init_config1 = {
        .unit_id = ADC_UNIT_1,
        .clk_src = ADC_DIGI_CLK_SRC_DEFAULT, // Clock source for digital controller
    };
    esp_err_t ret = adc_oneshot_new_unit(&init_config1, &adc1_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "adc_oneshot_new_unit failed: %s", esp_err_to_name(ret));
        vSemaphoreDelete(adc_mutex); // Clean up mutex if init fails
        adc_mutex = NULL;
        return false;
    }

    // ADC1 default channel (ECG) config
    adc_oneshot_chan_cfg_t config = {
        .bitwidth = ADC_BIT_WIDTH, // Should be ADC_BITWIDTH_12 or similar from esp_adc/adc_oneshot.h
        .atten = ADC_ATTEN,
    };
    ret = adc_oneshot_config_channel(adc1_handle, ADC_CHANNEL, &config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "adc_oneshot_config_channel for ECG failed: %s", esp_err_to_name(ret));
        adc_oneshot_del_unit(adc1_handle); // Clean up ADC unit
        vSemaphoreDelete(adc_mutex);
        adc_mutex = NULL;
        return false;
    }

    // Attempt calibration for the default ECG channel
    adc_cali_curve_fitting_config_t cali_config_ecg = {
        .unit_id = ADC_UNIT_1,
        .atten = ADC_ATTEN,
        .bitwidth = ADC_BIT_WIDTH, // Ensure this matches channel config
    };
    
    // Note: ADC_CHANNEL_GPIO_NUM should be defined in config.h or derived if ADC_CHANNEL is known
    // For example, if ADC_CHANNEL is ADC_CHANNEL_3, GPIO is 3 for ESP32-C3
    int ecg_gpio_num = -1; // Placeholder, set this appropriately
    #if CONFIG_IDF_TARGET_ESP32C3
    if (ADC_CHANNEL == ADC_CHANNEL_0) ecg_gpio_num = 0;
    else if (ADC_CHANNEL == ADC_CHANNEL_1) ecg_gpio_num = 1;
    else if (ADC_CHANNEL == ADC_CHANNEL_2) ecg_gpio_num = 2;
    else if (ADC_CHANNEL == ADC_CHANNEL_3) ecg_gpio_num = 3;
    else if (ADC_CHANNEL == ADC_CHANNEL_4) ecg_gpio_num = 4;
    #endif

    if (adc_cali_create_scheme_curve_fitting(&cali_config_ecg, &adc1_cali_handle) == ESP_OK) {
        do_calibration_ecg = true;
        ESP_LOGI(TAG, "ADC calibration enabled for ECG channel (GPIO%d)", ecg_gpio_num);
    } else {
        ESP_LOGW(TAG, "ADC calibration failed for ECG channel (GPIO%d)", ecg_gpio_num);
    }

    ESP_LOGI(TAG, "ADC module initialized successfully.");
    return true;
}

// Read AD8232 ECG signal
int adc_read_ecg_value(void) {
    if (adc_mutex == NULL) {
        ESP_LOGE(TAG, "ADC mutex not initialized in adc_read_ecg_value");
        return last_valid_ecg_adc; // Or some error code
    }

    int adc_raw;

    if (xSemaphoreTake(adc_mutex, pdMS_TO_TICKS(ADC_MUTEX_TIMEOUT_MS)) == pdTRUE) {
        static uint32_t error_count = 0;
        static uint32_t last_error_time = 0;
        uint32_t now = xTaskGetTickCount() * portTICK_PERIOD_MS;
        
        esp_err_t ret = adc_oneshot_read(adc1_handle, ADC_CHANNEL, &adc_raw);
        if (ret != ESP_OK) {
            error_count++;
            if (now - last_error_time > ADC_ERROR_LOG_INTERVAL_MS) { // Use config.h definition
                ESP_LOGW(TAG, "ECG ADC read error (%lu times): %s (code 0x%x)", 
                         (unsigned long)error_count, esp_err_to_name(ret), ret);
                last_error_time = now;
            }
            xSemaphoreGive(adc_mutex);
            return last_valid_ecg_adc; 
        }
        
        last_valid_ecg_adc = adc_raw; // Update last valid raw ADC value

        // ALWAYS return the raw ADC value for ECG processing by hr_calculation
        // and for consistent raw data logging/BLE transmission as per requirements.
        // The calibration to voltage can be performed by a separate function if needed elsewhere.
        /*
        // Original calibration logic - can be moved to a new function like adc_read_ecg_voltage_mv()
        if (do_calibration_ecg && adc1_cali_handle != NULL) {
            ret = adc_cali_raw_to_voltage(adc1_cali_handle, adc_raw, &voltage);
            if (ret == ESP_OK) {
                 xSemaphoreGive(adc_mutex);
                 return voltage; // Returns calibrated voltage if successful
            } else {
                ESP_LOGW(TAG, "ADC calibration failed for ECG channel, ret=%d. Returning raw value.", ret);
            }
        }
        */
        xSemaphoreGive(adc_mutex);
        return adc_raw; // Consistently return raw ADC value
    } else {
        ESP_LOGW(TAG, "Failed to take ADC mutex in adc_read_ecg_value");
        return last_valid_ecg_adc; // Or some error code
    }
}

// Release ADC resources
void adc_deinit(void) {
    if (adc_mutex != NULL) {
        // No need to take mutex here as we are deleting it
        if (do_calibration_ecg && adc1_cali_handle != NULL) {
            ESP_LOGI(TAG, "Deleting ECG ADC calibration scheme");
            adc_cali_delete_scheme_curve_fitting(adc1_cali_handle);
            adc1_cali_handle = NULL;
            do_calibration_ecg = false;
        }
        
        if (adc1_handle != NULL) {
             ESP_LOGI(TAG, "Deleting ADC unit");
            ESP_ERROR_CHECK(adc_oneshot_del_unit(adc1_handle));
            adc1_handle = NULL;
        }

        vSemaphoreDelete(adc_mutex);
        adc_mutex = NULL;
        ESP_LOGI(TAG, "ADC module de-initialized.");
    }
}

// Get ADC unit handle (shared with other modules)
adc_oneshot_unit_handle_t adc_get_handle(void) {
    // Access to handle should ideally also be mutex-protected if used for configuration elsewhere
    // However, for now, assuming it's mostly for read operations that will be individually protected.
    return adc1_handle;
}

// Configure ADC for an extra channel
bool adc_configure_extra_channel(adc_channel_t channel, adc_atten_t atten) {
    if (adc_mutex == NULL) {
        ESP_LOGE(TAG, "ADC mutex not initialized in adc_configure_extra_channel");
        return false;
    }
    if (adc1_handle == NULL) {
        ESP_LOGE(TAG, "ADC unit not initialized in adc_configure_extra_channel");
        return false;
    }

    bool success = false;
    if (xSemaphoreTake(adc_mutex, pdMS_TO_TICKS(ADC_MUTEX_TIMEOUT_MS)) == pdTRUE) {
        adc_oneshot_chan_cfg_t config = {
            .atten = atten,
            .bitwidth = ADC_BIT_WIDTH, // Assuming global bitwidth for all channels on ADC1
        };
        
        esp_err_t ret = adc_oneshot_config_channel(adc1_handle, channel, &config);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to configure ADC channel %d: %s", channel, esp_err_to_name(ret));
            // success remains false
        } else {
            ESP_LOGI(TAG, "Successfully configured ADC channel %d with atten %d", channel, atten);
            success = true;
        }
        xSemaphoreGive(adc_mutex);
    } else {
        ESP_LOGW(TAG, "Failed to take ADC mutex in adc_configure_extra_channel for channel %d", channel);
        // success remains false
    }
    return success;
}

// Generic protected ADC raw value reading function
bool adc_read_raw_protected(adc_channel_t channel, int *raw_value) {
    if (adc_mutex == NULL) {
        ESP_LOGE(TAG, "ADC mutex not initialized in adc_read_raw_protected for channel %d", channel);
        if (raw_value) *raw_value = 0; // Or some error indicator
        return false;
    }
    if (adc1_handle == NULL) {
        ESP_LOGE(TAG, "ADC unit not initialized in adc_read_raw_protected for channel %d", channel);
        if (raw_value) *raw_value = 0;
        return false;
    }
    if (raw_value == NULL) {
        ESP_LOGE(TAG, "Null pointer for raw_value in adc_read_raw_protected");
        return false;
    }

    bool success = false;
    if (xSemaphoreTake(adc_mutex, pdMS_TO_TICKS(ADC_MUTEX_TIMEOUT_MS)) == pdTRUE) {
        // Use a separate error counter for generic reads if desired, or a more complex per-channel error tracking
        static uint32_t error_count_generic = 0; 
        static uint32_t last_error_time_generic = 0;
        uint32_t now = xTaskGetTickCount() * portTICK_PERIOD_MS;

        esp_err_t ret = adc_oneshot_read(adc1_handle, channel, raw_value);
        if (ret == ESP_OK) {
            success = true;
        } else {
            error_count_generic++;
            if (now - last_error_time_generic > ADC_ERROR_LOG_INTERVAL_MS) {
                ESP_LOGW(TAG, "Generic ADC read error on channel %d (%lu times): %s (code 0x%x)",
                         channel, (unsigned long)error_count_generic, esp_err_to_name(ret), ret);
                last_error_time_generic = now;
            }
            // If read fails, *raw_value might be stale. The caller should check the return status.
        }
        xSemaphoreGive(adc_mutex);
    } else {
        ESP_LOGW(TAG, "Failed to take ADC mutex in adc_read_raw_protected for channel %d", channel);
        // If mutex fails, *raw_value is not updated.
    }
    return success;
}
