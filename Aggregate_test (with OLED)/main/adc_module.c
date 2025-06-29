#include <stdio.h>
#include "esp_log.h"
#include "esp_err.h"
#include "adc_module.h"

static const char *TAG = "ADC_MODULE";

// ADC handler
static adc_oneshot_unit_handle_t adc1_handle;
// Calibration handler
static adc_cali_handle_t adc1_cali_handle = NULL;
static bool do_calibration = false;

// u521du59cbu5316ADCu6a21u5757
bool adc_init(void) {
    // ADC1 init
    adc_oneshot_unit_init_cfg_t init_config1 = {
        .unit_id = ADC_UNIT_1,
    };
    ESP_ERROR_CHECK(adc_oneshot_new_unit(&init_config1, &adc1_handle));

    // ADC1 config
    adc_oneshot_chan_cfg_t config = {
        .bitwidth = ADC_BIT_WIDTH,
        .atten = ADC_ATTEN,
    };
    ESP_ERROR_CHECK(adc_oneshot_config_channel(adc1_handle, ADC_CHANNEL, &config));

    // u5c1du8bd5u8fdbu884cu6821u51c6
    adc_cali_handle_t handle = NULL;
    adc_cali_curve_fitting_config_t cali_config = {
        .unit_id = ADC_UNIT_1,
        .atten = ADC_ATTEN,
        .bitwidth = ADC_BIT_WIDTH,
    };
    
    if (adc_cali_create_scheme_curve_fitting(&cali_config, &handle) == ESP_OK) {
        do_calibration = true;
        adc1_cali_handle = handle;
        ESP_LOGI(TAG, "ADC calibration enabled");
    } else {
        ESP_LOGW(TAG, "No ADC calibration");
    }

    return true;
}

// u8bfbu53d6AD8232u5fc3u7535u4fe1u53f7
int adc_read_ecg_value(void) {
    int adc_raw;
    int voltage = 0;

    // Read ADC
    ESP_ERROR_CHECK(adc_oneshot_read(adc1_handle, ADC_CHANNEL, &adc_raw));
    
    // Convert to voltage if calibration is available
    if (do_calibration) {
        ESP_ERROR_CHECK(adc_cali_raw_to_voltage(adc1_cali_handle, adc_raw, &voltage));
        return voltage; // u8fd4u56deu6bebu4f0fu503c
    }
    
    return adc_raw; // u8fd4u56deu539fu59cbADCu503c
}

// u91cau653eADCu8d44u6e90
void adc_deinit(void) {
    if (do_calibration) {
        adc_cali_delete_scheme_curve_fitting(adc1_cali_handle);
    }
    
    ESP_ERROR_CHECK(adc_oneshot_del_unit(adc1_handle));
}
