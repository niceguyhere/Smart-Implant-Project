#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"
#include "esp_log.h"

static const char *TAG = "BATTERY_MONITOR";

// ADC configuration
#define ADC_ATTEN           ADC_ATTEN_DB_12     // 0-3.3V range
#define ADC_BITWIDTH        ADC_BITWIDTH_12     // 12-bit resolution (0-4095)
#define ADC_CHANNEL         ADC_CHANNEL_2       // GPIO2

// Battery voltage divider circuit parameters
#define R1_RESISTANCE       22000.0f            // R1 = 22kΩ (upper resistor)
#define R2_RESISTANCE       10000.0f            // R2 = 10kΩ (lower resistor)
#define DIVIDER_RATIO       ((R1_RESISTANCE + R2_RESISTANCE) / R2_RESISTANCE)  // (22k + 10k) / 10k = 3.2

// Battery parameters
#define BATTERY_MIN_VOLTAGE 3.0f                // Minimum safe battery voltage
#define BATTERY_MAX_VOLTAGE 4.2f                // Maximum battery voltage (fully charged)
#define BATTERY_NOMINAL     3.7f                // Nominal battery voltage

// Filtering parameters
#define FILTER_SAMPLES      10                  // Number of samples for averaging
#define ADC_SAMPLES         5                   // Multiple ADC reads per measurement

// Battery monitoring structure
typedef struct {
    int raw_adc;
    float adc_voltage;
    float battery_voltage;
    float battery_percentage;
} battery_data_t;

// Global variables
static adc_oneshot_unit_handle_t adc_handle;
static adc_cali_handle_t adc_cali_handle = NULL;
static bool adc_calibration_enabled = false;
static float voltage_buffer[FILTER_SAMPLES];
static int buffer_index = 0;
static bool buffer_filled = false;

// Initialize ADC calibration
static bool adc_calibration_init(void)
{
    esp_err_t ret = ESP_FAIL;
    bool calibrated = false;

#if ADC_CALI_SCHEME_CURVE_FITTING_SUPPORTED
    if (!calibrated) {
        ESP_LOGI(TAG, "Calibration scheme: Curve Fitting");
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
        ESP_LOGI(TAG, "Calibration scheme: Line Fitting");
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
        ESP_LOGI(TAG, "ADC calibration initialized successfully");
    } else {
        ESP_LOGW(TAG, "ADC calibration not available, using linear conversion");
    }
    
    return calibrated;
}

// Initialize ADC
static esp_err_t adc_init(void)
{
    esp_err_t ret;
    
    // ADC1 unit initialization
    adc_oneshot_unit_init_cfg_t init_config = {
        .unit_id = ADC_UNIT_1,
        .ulp_mode = ADC_ULP_MODE_DISABLE,
    };
    
    ret = adc_oneshot_new_unit(&init_config, &adc_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize ADC unit: %s", esp_err_to_name(ret));
        return ret;
    }
    
    // Configure ADC channel
    adc_oneshot_chan_cfg_t config = {
        .atten = ADC_ATTEN,
        .bitwidth = ADC_BITWIDTH,
    };
    
    ret = adc_oneshot_config_channel(adc_handle, ADC_CHANNEL, &config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to configure ADC channel: %s", esp_err_to_name(ret));
        return ret;
    }
    
    // Initialize ADC calibration
    adc_calibration_init();
    
    ESP_LOGI(TAG, "ADC initialized on GPIO2 (ADC1_CH2)");
    return ESP_OK;
}

// Read multiple ADC samples and return average
static int read_adc_averaged(void)
{
    int total = 0;
    int valid_samples = 0;
    
    for (int i = 0; i < ADC_SAMPLES; i++) {
        int raw_value;
        esp_err_t ret = adc_oneshot_read(adc_handle, ADC_CHANNEL, &raw_value);
        if (ret == ESP_OK) {
            total += raw_value;
            valid_samples++;
        }
        vTaskDelay(pdMS_TO_TICKS(1)); // Small delay between samples
    }
    
    return (valid_samples > 0) ? (total / valid_samples) : 0;
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
    
    // Fallback: linear conversion (assuming 3.3V reference)
    return (raw_value / 4095.0f) * 3.3f;
}

// Apply moving average filter
static float apply_filter(float new_voltage)
{
    // Add new sample to buffer
    voltage_buffer[buffer_index] = new_voltage;
    buffer_index = (buffer_index + 1) % FILTER_SAMPLES;
    
    if (!buffer_filled && buffer_index == 0) {
        buffer_filled = true;
    }
    
    // Calculate average
    float sum = 0.0f;
    int samples_count = buffer_filled ? FILTER_SAMPLES : buffer_index;
    
    for (int i = 0; i < samples_count; i++) {
        sum += voltage_buffer[i];
    }
    
    return sum / samples_count;
}

// Calculate battery percentage (simple linear approximation)
static float calculate_battery_percentage(float battery_voltage)
{
    if (battery_voltage >= BATTERY_MAX_VOLTAGE) {
        return 100.0f;
    } else if (battery_voltage <= BATTERY_MIN_VOLTAGE) {
        return 0.0f;
    } else {
        // Linear interpolation between min and max voltage
        return ((battery_voltage - BATTERY_MIN_VOLTAGE) / 
                (BATTERY_MAX_VOLTAGE - BATTERY_MIN_VOLTAGE)) * 100.0f;
    }
}

// Get battery status string
static const char* get_battery_status(float voltage, float percentage)
{
    if (voltage < BATTERY_MIN_VOLTAGE) {
        return "CRITICAL";
    } else if (percentage < 20.0f) {
        return "LOW";
    } else if (percentage < 50.0f) {
        return "MEDIUM";
    } else if (percentage < 80.0f) {
        return "GOOD";
    } else {
        return "EXCELLENT";
    }
}

// Read and process battery data
static esp_err_t read_battery_data(battery_data_t *data)
{
    // Read averaged ADC value
    data->raw_adc = read_adc_averaged();
    if (data->raw_adc == 0) {
        ESP_LOGE(TAG, "Failed to read ADC");
        return ESP_FAIL;
    }
    
    // Convert to ADC voltage
    data->adc_voltage = raw_to_voltage(data->raw_adc);
    
    // Calculate actual battery voltage using divider ratio
    float unfiltered_battery_voltage = data->adc_voltage * DIVIDER_RATIO;
    
    // Apply moving average filter
    data->battery_voltage = apply_filter(unfiltered_battery_voltage);
    
    // Calculate battery percentage
    data->battery_percentage = calculate_battery_percentage(data->battery_voltage);
    
    return ESP_OK;
}

// Task for battery monitoring
static void battery_monitor_task(void *pvParameters)
{
    battery_data_t battery_data;
    
    ESP_LOGI(TAG, "Starting battery voltage monitoring");
    ESP_LOGI(TAG, "Sample rate: 10Hz");
    ESP_LOGI(TAG, "Voltage divider: 22kΩ + 10kΩ (ratio: %.2f)", DIVIDER_RATIO);
    ESP_LOGI(TAG, "Battery range: %.1fV - %.1fV", BATTERY_MIN_VOLTAGE, BATTERY_MAX_VOLTAGE);
    ESP_LOGI(TAG, "Format: Raw ADC | ADC Voltage | Battery Voltage | Percentage | Status");
    
    while (1) {
        esp_err_t ret = read_battery_data(&battery_data);
        if (ret == ESP_OK) {
            const char* status = get_battery_status(battery_data.battery_voltage, 
                                                   battery_data.battery_percentage);
            
            printf("Battery: Raw=%4d | ADC=%5.3fV | Bat=%5.3fV | %5.1f%% | %s\n",
                   battery_data.raw_adc,
                   battery_data.adc_voltage,
                   battery_data.battery_voltage,
                   battery_data.battery_percentage,
                   status);
            
            // Log warnings for critical battery levels
            if (battery_data.battery_voltage < BATTERY_MIN_VOLTAGE) {
                ESP_LOGW(TAG, "CRITICAL: Battery voltage too low! %.3fV", 
                         battery_data.battery_voltage);
            } else if (battery_data.battery_percentage < 20.0f) {
                ESP_LOGW(TAG, "LOW BATTERY: %.1f%% remaining", 
                         battery_data.battery_percentage);
            }
        } else {
            printf("Battery: ERROR - Failed to read battery data\n");
        }
        
        // Wait 100ms for 10Hz sampling rate
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

void app_main(void)
{
    ESP_LOGI(TAG, "ESP32-C3 Battery Voltage Monitor Started");
    ESP_LOGI(TAG, "Circuit: Battery+ -> 22kΩ -> [ADC_GPIO2] -> 10kΩ -> GND");
    
    // Initialize ADC
    esp_err_t ret = adc_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "ADC initialization failed: %s", esp_err_to_name(ret));
        return;
    }
    
    // Wait for system stabilization
    vTaskDelay(pdMS_TO_TICKS(1000));
    
    // Create battery monitoring task
    xTaskCreate(battery_monitor_task, "battery_monitor", 4096, NULL, 5, NULL);
}
