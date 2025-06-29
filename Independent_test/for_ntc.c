#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"
#include "esp_log.h"

static const char *TAG = "NTC_TEMP";

// ADC configuration
#define ADC_ATTEN           ADC_ATTEN_DB_11     // 0-3.3V range
#define ADC_BITWIDTH        ADC_BITWIDTH_12     // 12-bit resolution (0-4095)

// NTC circuit parameters
#define VCC_VOLTAGE         3.3f                // Supply voltage
#define R_PULLUP            10000.0f            // Pull-up resistor 10kΩ
#define NTC_B_COEFFICIENT   3950.0f             // NTC B coefficient (typical value)
#define NTC_R25             10000.0f            // NTC resistance at 25°C (10kΩ)
#define TEMP_REFERENCE      25.0f               // Reference temperature (25°C)

// ADC channels
typedef enum {
    NTC_CHANNEL_1 = 0,
    NTC_CHANNEL_2,
    NTC_CHANNEL_MAX
} ntc_channel_t;

// NTC data structure
typedef struct {
    adc_channel_t adc_channel;
    const char *name;
    int raw_value;
    float voltage;
    float resistance;
    float temperature;
} ntc_sensor_t;

// Global variables
static adc_oneshot_unit_handle_t adc_handle;
static adc_cali_handle_t adc_cali_handle = NULL;
static bool adc_calibration_enabled = false;

// NTC sensor configuration
static ntc_sensor_t ntc_sensors[NTC_CHANNEL_MAX] = {
    {ADC_CHANNEL_0, "NTC1", 0, 0.0f, 0.0f, 0.0f},  // GPIO0
    {ADC_CHANNEL_2, "NTC2", 0, 0.0f, 0.0f, 0.0f}   // GPIO2
};

// Initialize ADC calibration
static bool adc_calibration_init(void)
{
    esp_err_t ret = ESP_FAIL;
    bool calibrated = false;

#if ADC_CALI_SCHEME_CURVE_FITTING_SUPPORTED
    if (!calibrated) {
        ESP_LOGI(TAG, "Calibration scheme version is Curve Fitting");
        adc_cali_curve_fitting_config_t cali_config = {
            .unit_id = ADC_UNIT_1,
            .atten = ADC_ATTEN,
            .bitwidth = ADC_BITWIDTH,
        };
        ret = adc_cali_create_scheme_curve_fitting(&cali_config, &adc_cali_handle);
        if (ret == ESP_OK) {
            calibrated = true;
        }
    }
#endif

#if ADC_CALI_SCHEME_LINE_FITTING_SUPPORTED
    if (!calibrated) {
        ESP_LOGI(TAG, "Calibration scheme version is Line Fitting");
        adc_cali_line_fitting_config_t cali_config = {
            .unit_id = ADC_UNIT_1,
            .atten = ADC_ATTEN,
            .bitwidth = ADC_BITWIDTH,
        };
        ret = adc_cali_create_scheme_line_fitting(&cali_config, &adc_cali_handle);
        if (ret == ESP_OK) {
            calibrated = true;
        }
    }
#endif

    adc_calibration_enabled = calibrated;
    if (calibrated) {
        ESP_LOGI(TAG, "ADC calibration initialized");
    } else {
        ESP_LOGW(TAG, "ADC calibration not available, using raw values");
    }
    
    return calibrated;
}

// Initialize ADC
static esp_err_t adc_init(void)
{
    esp_err_t ret;
    
    // ADC1 initialization
    adc_oneshot_unit_init_cfg_t init_config = {
        .unit_id = ADC_UNIT_1,
        .ulp_mode = ADC_ULP_MODE_DISABLE,
    };
    
    ret = adc_oneshot_new_unit(&init_config, &adc_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize ADC unit: %s", esp_err_to_name(ret));
        return ret;
    }
    
    // Configure ADC channels
    adc_oneshot_chan_cfg_t config = {
        .atten = ADC_ATTEN,
        .bitwidth = ADC_BITWIDTH,
    };
    
    for (int i = 0; i < NTC_CHANNEL_MAX; i++) {
        ret = adc_oneshot_config_channel(adc_handle, ntc_sensors[i].adc_channel, &config);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to configure ADC channel %d: %s", 
                     ntc_sensors[i].adc_channel, esp_err_to_name(ret));
            return ret;
        }
        ESP_LOGI(TAG, "%s configured on ADC channel %d", 
                 ntc_sensors[i].name, ntc_sensors[i].adc_channel);
    }
    
    // Initialize ADC calibration
    adc_calibration_init();
    
    ESP_LOGI(TAG, "ADC initialization completed");
    return ESP_OK;
}

// Read ADC raw value
static esp_err_t read_adc_raw(adc_channel_t channel, int *raw_value)
{
    return adc_oneshot_read(adc_handle, channel, raw_value);
}

// Convert ADC raw value to voltage
static float raw_to_voltage(int raw_value)
{
    if (adc_calibration_enabled) {
        int voltage_mv = 0;
        esp_err_t ret = adc_cali_raw_to_voltage(adc_cali_handle, raw_value, &voltage_mv);
        if (ret == ESP_OK) {
            return voltage_mv / 1000.0f; // Convert mV to V
        }
    }
    
    // Fallback: linear conversion
    return (raw_value / 4095.0f) * VCC_VOLTAGE;
}

// Calculate NTC resistance from voltage divider
static float calculate_ntc_resistance(float voltage)
{
    if (voltage >= VCC_VOLTAGE) {
        return 0.0f; // Avoid division by zero
    }
    
    // Voltage divider: V_ntc = VCC * R_ntc / (R_pullup + R_ntc)
    // Solving for R_ntc: R_ntc = R_pullup * V_ntc / (VCC - V_ntc)
    return R_PULLUP * voltage / (VCC_VOLTAGE - voltage);
}

// Convert NTC resistance to temperature using B coefficient equation
static float resistance_to_temperature(float resistance)
{
    if (resistance <= 0) {
        return -273.15f; // Invalid resistance
    }
    
    float temp_kelvin_inv = (1.0f / (TEMP_REFERENCE + 273.15f)) + 
                            (1.0f / NTC_B_COEFFICIENT) * log(resistance / NTC_R25);
    
    float temp_kelvin = 1.0f / temp_kelvin_inv;
    float temp_celsius = temp_kelvin - 273.15f;
    
    return temp_celsius;
}

// Read and process single NTC sensor
static esp_err_t read_ntc_sensor(ntc_sensor_t *sensor)
{
    esp_err_t ret = read_adc_raw(sensor->adc_channel, &sensor->raw_value);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to read %s: %s", sensor->name, esp_err_to_name(ret));
        return ret;
    }
    
    // Convert raw value to voltage
    sensor->voltage = raw_to_voltage(sensor->raw_value);
    
    // Calculate NTC resistance
    sensor->resistance = calculate_ntc_resistance(sensor->voltage);
    
    // Convert resistance to temperature
    sensor->temperature = resistance_to_temperature(sensor->resistance);
    
    return ESP_OK;
}

// Task for reading and displaying NTC data
static void ntc_reading_task(void *pvParameters)
{
    ESP_LOGI(TAG, "Starting NTC temperature reading task");
    ESP_LOGI(TAG, "Sample rate: 10Hz");
    ESP_LOGI(TAG, "Circuit: 3.3V - 10kΩ - [ADC] - NTC - GND");
    ESP_LOGI(TAG, "Format: Sensor | Raw ADC | Voltage | Resistance | Temperature");
    
    while (1) {
        printf("\n=== NTC Temperature Readings ===\n");
        
        // Read all NTC sensors
        for (int i = 0; i < NTC_CHANNEL_MAX; i++) {
            esp_err_t ret = read_ntc_sensor(&ntc_sensors[i]);
            if (ret == ESP_OK) {
                printf("%s | Raw: %4d | Voltage: %5.3fV | Resistance: %8.1fΩ | Temperature: %6.2f°C\n",
                       ntc_sensors[i].name,
                       ntc_sensors[i].raw_value,
                       ntc_sensors[i].voltage,
                       ntc_sensors[i].resistance,
                       ntc_sensors[i].temperature);
            } else {
                printf("%s | ERROR: Failed to read sensor\n", ntc_sensors[i].name);
            }
        }
        
        // Wait 100ms for 10Hz sampling rate
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

void app_main(void)
{
    ESP_LOGI(TAG, "ESP32-C3 Dual NTC Temperature Monitor Started");
    
    // Initialize ADC
    esp_err_t ret = adc_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "ADC initialization failed: %s", esp_err_to_name(ret));
        return;
    }
    
    // Wait for system stabilization
    vTaskDelay(pdMS_TO_TICKS(1000));
    
    // Create task for NTC reading
    xTaskCreate(ntc_reading_task, "ntc_reading_task", 4096, NULL, 5, NULL);
}
