#include <stdio.h>
#include <string.h>
#include <sys/unistd.h>
#include <sys/stat.h>
#include "esp_err.h"
#include "esp_log.h"
#include "esp_vfs_fat.h"
#include "driver/sdspi_host.h"
#include "driver/spi_common.h"
#include "sdmmc_cmd.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_timer.h"

static const char *TAG = "SD_CARD_TEST";

// SD card mount point
#define MOUNT_POINT "/sdcard"

// SPI pin definitions for ESP32-C3
#define PIN_NUM_MISO 5
#define PIN_NUM_MOSI 6
#define PIN_NUM_CLK  4
#define PIN_NUM_CS   7

// Global variables
static sdmmc_card_t *card;
static bool sd_mounted = false;

// Initialize and mount SD card
esp_err_t init_sd_card(void)
{
    esp_err_t ret;
    
    // Configure SPI bus
    spi_bus_config_t bus_cfg = {
        .mosi_io_num = PIN_NUM_MOSI,
        .miso_io_num = PIN_NUM_MISO,
        .sclk_io_num = PIN_NUM_CLK,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = 4000,
    };
    
    ret = spi_bus_initialize(SPI2_HOST, &bus_cfg, SDSPI_DEFAULT_DMA);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "SPI bus initialization failed: %s", esp_err_to_name(ret));
        return ret;
    }
    
    ESP_LOGI(TAG, "SPI bus initialized successfully");
    
    // Configure mount options
    esp_vfs_fat_sdmmc_mount_config_t mount_config = {
        .format_if_mount_failed = false,
        .max_files = 5,
        .allocation_unit_size = 16 * 1024
    };
    
    // Configure SD card host
    sdmmc_host_t host = SDSPI_HOST_DEFAULT();
    host.slot = SPI2_HOST;
    
    // Configure SD card slot
    sdspi_device_config_t slot_config = SDSPI_DEVICE_CONFIG_DEFAULT();
    slot_config.gpio_cs = PIN_NUM_CS;
    slot_config.host_id = host.slot;
    
    // Mount SD card
    ESP_LOGI(TAG, "Mounting SD card...");
    ret = esp_vfs_fat_sdspi_mount(MOUNT_POINT, &host, &slot_config, &mount_config, &card);
    
    if (ret != ESP_OK) {
        if (ret == ESP_FAIL) {
            ESP_LOGE(TAG, "Failed to mount filesystem. If you want to format the SD card, set format_if_mount_failed = true");
        } else {
            ESP_LOGE(TAG, "Failed to initialize SD card (%s). Make sure SD card is inserted and connections are correct", esp_err_to_name(ret));
        }
        return ret;
    }
    
    sd_mounted = true;
    
    // Print SD card information
    ESP_LOGI(TAG, "SD card mounted successfully!");
    sdmmc_card_print_info(stdout, card);
    
    // Get SD card capacity information
    uint64_t card_size = ((uint64_t) card->csd.capacity) * card->csd.sector_size;
    ESP_LOGI(TAG, "SD card total capacity: %lluMB", card_size / (1024 * 1024));
    
    FATFS *fs;
    DWORD fre_clust, fre_sect, tot_sect;
    
    // Get free space information
    if (f_getfree("0:", &fre_clust, &fs) == FR_OK) {
        tot_sect = (fs->n_fatent - 2) * fs->csize;
        fre_sect = fre_clust * fs->csize;
        ESP_LOGI(TAG, "Total sectors: %lu, Free sectors: %lu", tot_sect, fre_sect);
        ESP_LOGI(TAG, "Total space: %luMB, Free space: %luMB", 
                 tot_sect / (2 * 1024), fre_sect / (2 * 1024));
    }
    
    return ESP_OK;
}

// Create data folder
esp_err_t create_data_folder(void)
{
    char folder_path[64];
    snprintf(folder_path, sizeof(folder_path), "%s/data", MOUNT_POINT);
    
    struct stat st = {0};
    if (stat(folder_path, &st) == -1) {
        if (mkdir(folder_path, 0700) == 0) {
            ESP_LOGI(TAG, "Data folder created successfully: %s", folder_path);
        } else {
            ESP_LOGE(TAG, "Failed to create data folder: %s", folder_path);
            return ESP_FAIL;
        }
    } else {
        ESP_LOGI(TAG, "Data folder already exists: %s", folder_path);
    }
    
    return ESP_OK;
}

// Get current timestamp in milliseconds
uint64_t get_timestamp_ms(void)
{
    return esp_timer_get_time() / 1000;
}

// Write timestamp file
esp_err_t write_timestamp_file(void)
{
    if (!sd_mounted) {
        ESP_LOGE(TAG, "SD card not mounted, cannot write file");
        return ESP_FAIL;
    }
    
    // Get current timestamp
    uint64_t timestamp = get_timestamp_ms();
    
    // Build file path
    char file_path[128];
    snprintf(file_path, sizeof(file_path), "%s/data/%llu.txt", MOUNT_POINT, timestamp);
    
    // Open file for writing
    FILE *file = fopen(file_path, "w");
    if (file == NULL) {
        ESP_LOGE(TAG, "Cannot create file: %s", file_path);
        return ESP_FAIL;
    }
    
    // Write timestamp content
    fprintf(file, "Timestamp: %llu ms\n", timestamp);
    fprintf(file, "System uptime: %llu ms\n", esp_timer_get_time() / 1000);
    
    // Close file
    fclose(file);
    
    ESP_LOGI(TAG, "File written successfully: %s (timestamp: %llu)", file_path, timestamp);
    
    // Verify file was written successfully
    struct stat file_stat;
    if (stat(file_path, &file_stat) == 0) {
        ESP_LOGI(TAG, "File size: %ld bytes", file_stat.st_size);
        return ESP_OK;
    } else {
        ESP_LOGE(TAG, "File write verification failed");
        return ESP_FAIL;
    }
}

// Periodic write task
void timestamp_write_task(void *pvParameters)
{
    ESP_LOGI(TAG, "Timestamp write task started, writing files every 30 seconds");
    
    while (1) {
        if (sd_mounted) {
            esp_err_t result = write_timestamp_file();
            if (result == ESP_OK) {
                ESP_LOGI(TAG, "✓ File write successful");
            } else {
                ESP_LOGE(TAG, "✗ File write failed");
            }
        } else {
            ESP_LOGW(TAG, "SD card not mounted, skipping write");
        }
        
        // Wait for 30 seconds
        vTaskDelay(30000 / portTICK_PERIOD_MS);
    }
}

// Unmount SD card
void deinit_sd_card(void)
{
    if (sd_mounted) {
        esp_vfs_fat_sdcard_unmount(MOUNT_POINT, card);
        ESP_LOGI(TAG, "SD card unmounted");
        sd_mounted = false;
    }
    
    spi_bus_free(SPI2_HOST);
    ESP_LOGI(TAG, "SPI bus freed");
}

void app_main(void)
{
    ESP_LOGI(TAG, "=== ESP32-C3 SD Card Test Program Started ===");
    ESP_LOGI(TAG, "Pin configuration: SCK-GPIO%d, MISO-GPIO%d, MOSI-GPIO%d, CS-GPIO%d", 
             PIN_NUM_CLK, PIN_NUM_MISO, PIN_NUM_MOSI, PIN_NUM_CS);
    
    // Initialize and mount SD card
    esp_err_t ret = init_sd_card();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "SD card initialization failed, program exit");
        return;
    }
    
    // Create data folder
    ret = create_data_folder();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create data folder, program exit");
        deinit_sd_card();
        return;
    }
    
    // Write an initial test file immediately
    ESP_LOGI(TAG, "Writing initial test file...");
    ret = write_timestamp_file();
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "✓ Initial test file written successfully");
    } else {
        ESP_LOGE(TAG, "✗ Initial test file write failed");
    }
    
    // Create periodic write task
    xTaskCreate(timestamp_write_task, "timestamp_write", 4096, NULL, 5, NULL);
    
    ESP_LOGI(TAG, "=== SD Card test program running, automatically writing files every 30 seconds ===");
    
    // Main loop - can add other functions here
    while (1) {
        vTaskDelay(10000 / portTICK_PERIOD_MS); // Print status every 10 seconds
        ESP_LOGI(TAG, "Program running... System uptime: %llu ms", esp_timer_get_time() / 1000);
    }
}
