#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_log.h"

static const char *TAG = "GPIO_TEST";

// Define GPIO pins to test
static const int gpio_pins[] = {
    GPIO_NUM_0, GPIO_NUM_1, GPIO_NUM_2, GPIO_NUM_3, GPIO_NUM_4,
    GPIO_NUM_5, GPIO_NUM_6, GPIO_NUM_7, GPIO_NUM_8, GPIO_NUM_9,
    GPIO_NUM_10, GPIO_NUM_20, GPIO_NUM_21
};

#define GPIO_COUNT (sizeof(gpio_pins) / sizeof(gpio_pins[0]))

void gpio_init_output(void)
{
    ESP_LOGI(TAG, "Initialize GPIO pins as output mode");
    
    for (int i = 0; i < GPIO_COUNT; i++) {
        gpio_config_t io_conf = {
            .intr_type = GPIO_INTR_DISABLE,          // Disable interrupt
            .mode = GPIO_MODE_OUTPUT,                // Set as output mode
            .pin_bit_mask = (1ULL << gpio_pins[i]),  // Set pin bit mask
            .pull_down_en = GPIO_PULLDOWN_DISABLE,   // Disable pull-down
            .pull_up_en = GPIO_PULLUP_DISABLE        // Disable pull-up
        };
        
        esp_err_t ret = gpio_config(&io_conf);
        if (ret == ESP_OK) {
            ESP_LOGI(TAG, "GPIO%d configured successfully", gpio_pins[i]);
        } else {
            ESP_LOGE(TAG, "GPIO%d configuration failed: %s", gpio_pins[i], esp_err_to_name(ret));
        }
    }
}

void gpio_set_all_high(void)
{
    ESP_LOGI(TAG, "Set all GPIO to high level");
    
    for (int i = 0; i < GPIO_COUNT; i++) {
        esp_err_t ret = gpio_set_level(gpio_pins[i], 1);
        if (ret == ESP_OK) {
            ESP_LOGI(TAG, "GPIO%d set to high level", gpio_pins[i]);
        } else {
            ESP_LOGE(TAG, "GPIO%d set failed: %s", gpio_pins[i], esp_err_to_name(ret));
        }
    }
}

void gpio_toggle_test(void)
{
    ESP_LOGI(TAG, "Start GPIO level toggle test");
    
    while (1) {
        // Set all GPIO to high level
        ESP_LOGI(TAG, "Set all GPIO to high level");
        for (int i = 0; i < GPIO_COUNT; i++) {
            gpio_set_level(gpio_pins[i], 1);
        }
        vTaskDelay(pdMS_TO_TICKS(2000)); // Delay 2 seconds
        
        // Set all GPIO to low level
        ESP_LOGI(TAG, "Set all GPIO to low level");
        for (int i = 0; i < GPIO_COUNT; i++) {
            gpio_set_level(gpio_pins[i], 0);
        }
        vTaskDelay(pdMS_TO_TICKS(2000)); // Delay 2 seconds
    }
}

void app_main(void)
{
    ESP_LOGI(TAG, "ESP32-C3 GPIO Test Program Started");
    ESP_LOGI(TAG, "Testing pins: GPIO0-10, GPIO20, GPIO21 (Total %d pins)", GPIO_COUNT);
    
    // Initialize GPIO as output mode
    gpio_init_output();
    
    // Wait 1 second
    vTaskDelay(pdMS_TO_TICKS(1000));
    
    // Set all GPIO to high level (as requested)
    gpio_set_all_high();
    
    ESP_LOGI(TAG, "All GPIO set to high level, starting continuous level toggle test");
    ESP_LOGI(TAG, "You can measure pin levels with multimeter or oscilloscope");
    
    // Start toggle test (optional, for observation)
    gpio_toggle_test();
}

