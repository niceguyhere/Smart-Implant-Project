#include <string.h>
#include <sys/unistd.h>
#include <sys/stat.h>
#include <errno.h>
#include "esp_vfs_fat.h"
#include "sdmmc_cmd.h"
#include "driver/spi_common.h"
#include "esp_log.h"
#include "sd_card.h" // Header for this module, includes DoubleBuffer_t, ECGData_t etc.

// New includes for the added functionality
#include "config.h"          // For ECG_SAMPLE_RATE_HZ, MPU_AWAKE_SAMPLE_RATE_HZ etc.
#include "freertos/task.h"   // For xTaskGetTickCount, pdMS_TO_TICKS, task creation
#include "esp_heap_caps.h"   // For heap_caps_malloc
#include <time.h>            // For time functions used in create_filename
#include <inttypes.h>        // For PRIu32

static const char *TAG = "SD_CARD"; // Re-using TAG for now

// --- Start of new definitions and globals ---

// SD卡初始化重试间隔(毫秒)
#define SD_INIT_RETRY_INTERVAL_MS 30000

// 缓冲区配置 (from main.c, uses defines from config.h)
#define BUFFER_DURATION_SEC 5  // 5秒缓冲
#define ECG_BUFFER_SIZE (ECG_SAMPLE_RATE_HZ * BUFFER_DURATION_SEC)
#define LEAD_BUFFER_SIZE (1000 / ELECTRODE_CHECK_INTERVAL_MS * BUFFER_DURATION_SEC)
#define MPU_BUFFER_SIZE (MPU_AWAKE_SAMPLE_RATE_HZ * BUFFER_DURATION_SEC)
#define TEMP_BUFFER_SIZE (1000 / MPU_TEMP_INTERVAL_MS * BUFFER_DURATION_SEC)
#define BATT_BUFFER_SIZE (1000 / BATT_SAMPLE_INTERVAL * BUFFER_DURATION_SEC + 1)

// 数据目录定义 (from main.c)
// SD_MOUNT_POINT is already defined in sd_card.h, so BASE_PATH uses it.
#define BASE_PATH SD_MOUNT_POINT 
#define ECG_DIR BASE_PATH"/ecg"
#define LEAD_DIR BASE_PATH"/lead"
#define MPU_DIR BASE_PATH"/mpu"
#define TEMP_DIR BASE_PATH"/temp"
#define BATT_DIR BASE_PATH"/battery"

// 全局缓冲区 (from main.c, type changed to DoubleBuffer_t)
static DoubleBuffer_t ecg_buffer;
static DoubleBuffer_t lead_buffer;
static DoubleBuffer_t mpu_buffer;
static DoubleBuffer_t temp_buffer;
static DoubleBuffer_t batt_buffer;

// Task handles for tasks to be defined in this module
static TaskHandle_t s_data_writer_task_handle = NULL;


// --- End of new definitions and globals ---
static sdmmc_card_t *s_card = NULL;
static bool s_card_mounted = false;

bool sd_card_init(bool format_if_failed) {
    if (s_card_mounted) {
        ESP_LOGI(TAG, "SD card already mounted");
        return true;
    }

    esp_err_t ret;
    
    // 始终不自动格式化，忽略传入参数
    esp_vfs_fat_sdmmc_mount_config_t mount_config = {
        .format_if_mount_failed = false,  // 始终不自动格式化SD卡
        .max_files = 5,
        .allocation_unit_size = 16 * 1024
    };
    
    ESP_LOGI(TAG, "Initializing SD card");
    ESP_LOGI(TAG, "Using SPI peripheral");
    
    // SPIu603bu7ebfu914du7f6e
    sdmmc_host_t host = SDSPI_HOST_DEFAULT();
    
    spi_bus_config_t bus_cfg = {
        .mosi_io_num = SD_PIN_MOSI,
        .miso_io_num = SD_PIN_MISO,
        .sclk_io_num = SD_PIN_CLK,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = 4000,
    };
    
    // u521du59cbu5316SPIu603bu7ebf
    ret = spi_bus_initialize(host.slot, &bus_cfg, SDSPI_DEFAULT_DMA);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize bus. Error: %s", esp_err_to_name(ret));
        return false;
    }
    
    // u914du7f6eSDu5361SPIu8bbeu5907
    sdspi_device_config_t slot_config = SDSPI_DEVICE_CONFIG_DEFAULT();
    slot_config.gpio_cs = SD_PIN_CS;
    slot_config.host_id = host.slot;
    
    // u6302u8f7du6587u4ef6u7cfbu7edf
    ESP_LOGI(TAG, "Mounting filesystem");
    ret = esp_vfs_fat_sdspi_mount(SD_MOUNT_POINT, &host, &slot_config, &mount_config, &s_card);
    
    if (ret != ESP_OK) {
        if (ret == ESP_FAIL) {
            ESP_LOGE(TAG, "Failed to mount filesystem. "
                    "Check SD card or enable format_if_mount_failed option.");
        } else {
            ESP_LOGE(TAG, "Failed to initialize the card (%s). "
                    "Make sure SD card is inserted correctly.", esp_err_to_name(ret));
        }
        return false;
    }
    
    s_card_mounted = true;
    ESP_LOGI(TAG, "Filesystem mounted");
    
    // u6253u5370SDu5361u4fe1u606f
    sdmmc_card_print_info(stdout, s_card);
    
    return true;
}

esp_err_t sd_card_write_file(const char *filename, const char *data) {
    if (!s_card_mounted) {
        ESP_LOGE(TAG, "SD card not mounted");
        return ESP_ERR_INVALID_STATE;
    }
    
    ESP_LOGI(TAG, "Writing file: %s", filename);
    FILE *f = fopen(filename, "w");
    if (f == NULL) {
        ESP_LOGE(TAG, "Failed to open file for writing");
        return ESP_FAIL;
    }
    
    fprintf(f, "%s", data);
    fclose(f);
    
    ESP_LOGI(TAG, "File written successfully");
    return ESP_OK;
}

esp_err_t sd_card_read_file(const char *filename, char *data, size_t max_size) {
    if (!s_card_mounted) {
        ESP_LOGE(TAG, "SD card not mounted");
        return ESP_ERR_INVALID_STATE;
    }
    
    ESP_LOGI(TAG, "Reading file: %s", filename);
    FILE *f = fopen(filename, "r");
    if (f == NULL) {
        ESP_LOGE(TAG, "Failed to open file for reading");
        return ESP_FAIL;
    }
    
    size_t bytes_read = fread(data, 1, max_size - 1, f);
    data[bytes_read] = '\0'; // u786eu4fddu5b57u7b26u4e32u7ed3u5c3e
    fclose(f);
    
    ESP_LOGI(TAG, "Read from file: '%s'", data);
    return ESP_OK;
}

void sd_card_deinit(void) {
    // 释放资源时先检查卡是否已挂载
    if (s_card_mounted && s_card != NULL) {
        // 卸载SD卡
        esp_vfs_fat_sdcard_unmount(SD_MOUNT_POINT, s_card);
        ESP_LOGI(TAG, "Card unmounted");
        s_card = NULL;
        s_card_mounted = false;
    } else {
        ESP_LOGD(TAG, "Card not mounted, skipping unmount");
    }
    
    // 尝试释放SPI总线（即使之前可能没有初始化成功）
    // 使用强制释放可能导致错误，但可以确保下次初始化时总线是空闲的
    esp_err_t ret = spi_bus_free(SDSPI_DEFAULT_HOST);
    if (ret == ESP_OK) {
        ESP_LOGD(TAG, "SPI bus released");
    } else if (ret == ESP_ERR_INVALID_STATE) {
        ESP_LOGD(TAG, "SPI bus was not initialized or already freed");
    } else {
        ESP_LOGW(TAG, "Error releasing SPI bus: %s", esp_err_to_name(ret));
    }
}

// 递归创建目录
esp_err_t sd_mkdirs(const char *path) {
    char tmp[256];
    char *p = NULL;
    size_t len;

    snprintf(tmp, sizeof(tmp), "%s", path);
    len = strlen(tmp);
    if(tmp[len - 1] == '/') tmp[len - 1] = 0;
    
    for(p = tmp + 1; *p; p++) {
        if(*p == '/') {
            *p = 0;
            if(mkdir(tmp, 0777) != 0 && errno != EEXIST) {
                ESP_LOGE(TAG, "创建目录失败: %s", tmp);
                return ESP_FAIL;
            }
            *p = '/';
        }
    }
    
    if(mkdir(tmp, 0777) != 0 && errno != EEXIST) {
        ESP_LOGE(TAG, "创建最终目录失败: %s", tmp);
        return ESP_FAIL;
    }
    return ESP_OK;
}

// 检查SD卡是否已挂载
bool sd_card_mounted(void) {
    return s_card_mounted;
}

// --- Start of new functions added to sd_card.c ---

// 初始化双缓冲区 (adapted from main.c's init_double_buffer)
static void sd_module_init_double_buffer(DoubleBuffer_t *buf, size_t element_size, uint16_t capacity) {
    buf->buffer1 = heap_caps_malloc(element_size * capacity, MALLOC_CAP_8BIT);
    buf->buffer2 = heap_caps_malloc(element_size * capacity, MALLOC_CAP_8BIT);
    
    if (!buf->buffer1 || !buf->buffer2) {
        ESP_LOGE(TAG, "Failed to allocate memory for double buffer (element_size %zu, capacity %u)", element_size, capacity);
        buf->capacity = 0; // Indicate buffer is unusable
        buf->mutex = NULL;
        // Ensure other fields are also in a safe state if needed
        buf->active_buf = NULL;
        buf->ready_buf = NULL;
        buf->count = 0;
        return;
    }
    
    buf->active_buf = buf->buffer1;
    buf->ready_buf = buf->buffer2;
    buf->count = 0;
    buf->capacity = capacity;
    buf->mutex = xSemaphoreCreateMutex();
    if (buf->mutex == NULL) {
        ESP_LOGE(TAG, "Failed to create mutex for double buffer");
        heap_caps_free(buf->buffer1);
        heap_caps_free(buf->buffer2);
        buf->buffer1 = NULL;
        buf->buffer2 = NULL;
        buf->active_buf = NULL;
        buf->ready_buf = NULL;
        buf->capacity = 0;
        buf->count = 0;
        return;
    }
    ESP_LOGI(TAG, "Initialized double buffer: capacity %u, element_size %zu bytes", capacity, element_size);
}

// Public function to initialize all data buffers
void sd_buffers_init(void) {
    sd_module_init_double_buffer(&ecg_buffer, sizeof(ECGData_t), ECG_BUFFER_SIZE);
    sd_module_init_double_buffer(&lead_buffer, sizeof(LeadData_t), LEAD_BUFFER_SIZE);
    sd_module_init_double_buffer(&mpu_buffer, sizeof(MPUData_t), MPU_BUFFER_SIZE);
    sd_module_init_double_buffer(&temp_buffer, sizeof(TempData_t), TEMP_BUFFER_SIZE);
    sd_module_init_double_buffer(&batt_buffer, sizeof(BattData_t), BATT_BUFFER_SIZE);
    ESP_LOGI(TAG, "All SD data buffers initialized.");
}

// 交换缓冲区 (adapted from main.c's swap_buffer)
static uint16_t sd_module_swap_buffer(DoubleBuffer_t *buf) {
    if (buf->mutex == NULL || buf->capacity == 0) {
        ESP_LOGW(TAG, "Swap buffer called on uninitialized or failed buffer.");
        return 0; 
    }
    if (xSemaphoreTake(buf->mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        void *temp_ptr = buf->active_buf;
        buf->active_buf = buf->ready_buf;
        buf->ready_buf = temp_ptr;
        
        uint16_t items_in_ready_buffer = buf->count;
        buf->count = 0; // Reset count for the new active buffer.

        xSemaphoreGive(buf->mutex);
        return items_in_ready_buffer;
    }
    ESP_LOGW(TAG, "Failed to take mutex for swapping buffer");
    return 0;
}

// --- Implementations for sd_add_..._data functions ---

void sd_add_ecg_data(int16_t value) {
    if (ecg_buffer.mutex == NULL || ecg_buffer.capacity == 0) return;
    if (xSemaphoreTake(ecg_buffer.mutex, pdMS_TO_TICKS(10)) == pdTRUE) {
        if (ecg_buffer.count < ecg_buffer.capacity) {
            ECGData_t *data_ptr = (ECGData_t*)ecg_buffer.active_buf + ecg_buffer.count;
            gettimeofday(&data_ptr->timestamp, NULL);
            data_ptr->value = value;
            ecg_buffer.count++;
        } else {
            // ESP_LOGW(TAG, "ECG buffer full, data dropped"); // Optional: log if buffer is full
        }
        xSemaphoreGive(ecg_buffer.mutex);
    }
}

void sd_add_lead_data(uint8_t status) {
    if (lead_buffer.mutex == NULL || lead_buffer.capacity == 0) return;
    if (xSemaphoreTake(lead_buffer.mutex, pdMS_TO_TICKS(10)) == pdTRUE) {
        if (lead_buffer.count < lead_buffer.capacity) {
            LeadData_t *data_ptr = (LeadData_t*)lead_buffer.active_buf + lead_buffer.count;
            gettimeofday(&data_ptr->timestamp, NULL);
            data_ptr->status = status;
            lead_buffer.count++;
        } 
        xSemaphoreGive(lead_buffer.mutex);
    }
}

void sd_add_mpu_data(int16_t accel_x, int16_t accel_y, int16_t accel_z,
                     int16_t gyro_x, int16_t gyro_y, int16_t gyro_z) {
    if (mpu_buffer.mutex == NULL || mpu_buffer.capacity == 0) return;
    if (xSemaphoreTake(mpu_buffer.mutex, pdMS_TO_TICKS(10)) == pdTRUE) {
        if (mpu_buffer.count < mpu_buffer.capacity) {
            MPUData_t *data_ptr = (MPUData_t*)mpu_buffer.active_buf + mpu_buffer.count;
            gettimeofday(&data_ptr->timestamp, NULL);
            data_ptr->accel_x = accel_x; data_ptr->accel_y = accel_y; data_ptr->accel_z = accel_z;
            data_ptr->gyro_x = gyro_x; data_ptr->gyro_y = gyro_y; data_ptr->gyro_z = gyro_z;
            mpu_buffer.count++;
        }
        xSemaphoreGive(mpu_buffer.mutex);
    }
}

void sd_add_temp_data(int16_t mpu_temp_scaled, int16_t ntc1_temp_scaled, int16_t ntc2_temp_scaled) {
    if (temp_buffer.mutex == NULL || temp_buffer.capacity == 0) return;
    if (xSemaphoreTake(temp_buffer.mutex, pdMS_TO_TICKS(10)) == pdTRUE) {
        if (temp_buffer.count < temp_buffer.capacity) {
            TempData_t *data_ptr = (TempData_t*)temp_buffer.active_buf + temp_buffer.count;
            gettimeofday(&data_ptr->timestamp, NULL);
            data_ptr->mpu_temp = mpu_temp_scaled;
            data_ptr->ntc1_temp = ntc1_temp_scaled;
            data_ptr->ntc2_temp = ntc2_temp_scaled;
            temp_buffer.count++;
        }
        xSemaphoreGive(temp_buffer.mutex);
    }
}

void sd_add_batt_data(uint16_t voltage_mv, uint8_t soc_percent) {
    if (batt_buffer.mutex == NULL || batt_buffer.capacity == 0) return;
    if (xSemaphoreTake(batt_buffer.mutex, pdMS_TO_TICKS(10)) == pdTRUE) {
        if (batt_buffer.count < batt_buffer.capacity) {
            BattData_t *data_ptr = (BattData_t*)batt_buffer.active_buf + batt_buffer.count;
            gettimeofday(&data_ptr->timestamp, NULL);
            data_ptr->voltage = voltage_mv;
            data_ptr->soc = soc_percent;
            batt_buffer.count++;
        }
        xSemaphoreGive(batt_buffer.mutex);
    }
}

// --- End of first block of new functions ---

// --- Start of file writing functions and task ---

// 创建文件名 (格式: 年月日_小时.csv) - (adapted from main.c)
// Note: This function now uses the TAG from sd_card.c
static void sd_module_create_filename(char *filename, size_t size, const char *dir, const char *prefix) {
    time_t now = time(NULL);
    struct tm timeinfo;
    // ESP-IDF typically uses a thread-safe version of localtime if available, or ensure proper sync if not.
    // For simplicity, using localtime_r if available, otherwise consider mutex for 'now' and 'timeinfo' if shared.
    localtime_r(&now, &timeinfo); 
    
    snprintf(filename, size, "%s/%s_%04d%02d%02d_%02d.csv", 
             dir, prefix, 
             timeinfo.tm_year + 1900, timeinfo.tm_mon + 1, timeinfo.tm_mday, 
             timeinfo.tm_hour);
}

// 写入心电数据到SD卡(CSV格式) - (adapted from main.c)
static void sd_module_write_ecg_data() {
    uint16_t count = sd_module_swap_buffer(&ecg_buffer);
    if (count == 0) {
        ESP_LOGD(TAG, "ECG buffer empty, skipping write");
        return;
    }
    
    struct stat st;
    if (stat(ECG_DIR, &st) != 0) {
        ESP_LOGW(TAG, "ECG data directory does not exist: %s. Attempting to create.", ECG_DIR);
        if (sd_mkdirs(ECG_DIR) != ESP_OK) {
            ESP_LOGE(TAG, "Failed to create ECG data directory: %s (errno: %d - %s)", ECG_DIR, errno, strerror(errno));
            return;
        }
    }
    
    char filename[64];
    sd_module_create_filename(filename, sizeof(filename), ECG_DIR, "ecg");
    
    bool file_exists = (access(filename, F_OK) == 0);
    FILE *f = fopen(filename, "a");
    if (f) {
        if (!file_exists) {
            fprintf(f, "timestamp,ecg_value\n");
        }
        ECGData_t *data = (ECGData_t*)ecg_buffer.ready_buf;
        for (int i = 0; i < count; i++) {
            struct tm timeinfo;
            char time_str[30];
            localtime_r(&data[i].timestamp.tv_sec, &timeinfo);
            strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", &timeinfo);
            long milliseconds = data[i].timestamp.tv_usec / 1000;
            fprintf(f, "%s.%03ld,%d\n", time_str, milliseconds, data[i].value);
            if ((i + 1) % 50 == 0) { // Yield every 50 records
                vTaskDelay(pdMS_TO_TICKS(1));
            }
        }
        vTaskDelay(pdMS_TO_TICKS(1)); // Yield before closing file
        fclose(f);
        ESP_LOGD(TAG, "Successfully wrote %d ECG data points to %s", count, filename);
    } else {
        ESP_LOGE(TAG, "Failed to open ECG data file: %s (errno: %d - %s)", filename, errno, strerror(errno));
    }
}

// 写入导联状态数据到SD卡(CSV格式) - (adapted from main.c)
static void sd_module_write_lead_data() {
    uint16_t count = sd_module_swap_buffer(&lead_buffer);
    if (count == 0) return;
    
    // Directory creation should be handled by data_writer_task or init
    char filename[64];
    sd_module_create_filename(filename, sizeof(filename), LEAD_DIR, "lead");
    bool file_exists = (access(filename, F_OK) == 0);
    FILE *f = fopen(filename, "a");
    if (f) {
        if (!file_exists) {
            fprintf(f, "timestamp,lead_status\n");
        }
        LeadData_t *data = (LeadData_t*)lead_buffer.ready_buf;
        for (int i = 0; i < count; i++) {
            struct tm timeinfo;
            char time_str[30];
            localtime_r(&data[i].timestamp.tv_sec, &timeinfo);
            strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", &timeinfo);
            long milliseconds = data[i].timestamp.tv_usec / 1000;
            fprintf(f, "%s.%03ld,%u\n", time_str, milliseconds, data[i].status);
            if ((i + 1) % 50 == 0) { // Yield every 50 records
                vTaskDelay(pdMS_TO_TICKS(1));
            }
        }
        vTaskDelay(pdMS_TO_TICKS(1)); // Yield before closing file
        fclose(f);
        ESP_LOGD(TAG, "Wrote %d lead status data to %s", count, filename);
    } else {
        ESP_LOGE(TAG, "Failed to open lead data file: %s (errno: %d - %s)", filename, errno, strerror(errno));
    }
}

// 写入MPU数据到SD卡(CSV格式) - (adapted from main.c)
static void sd_module_write_mpu_data() {
    uint16_t count = sd_module_swap_buffer(&mpu_buffer);
    if (count == 0) return;

    char filename[64];
    sd_module_create_filename(filename, sizeof(filename), MPU_DIR, "mpu");
    bool file_exists = (access(filename, F_OK) == 0);
    FILE *f = fopen(filename, "a");
    if (f) {
        if (!file_exists) {
            fprintf(f, "timestamp,accel_x,accel_y,accel_z,gyro_x,gyro_y,gyro_z\n");
        }
        MPUData_t *data = (MPUData_t*)mpu_buffer.ready_buf;
        for (int i = 0; i < count; i++) {
            struct tm timeinfo;
            char time_str[30];
            localtime_r(&data[i].timestamp.tv_sec, &timeinfo);
            strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", &timeinfo);
            long milliseconds = data[i].timestamp.tv_usec / 1000;
            fprintf(f, "%s.%03ld,%d,%d,%d,%d,%d,%d\n", 
                    time_str, milliseconds, 
                    data[i].accel_x, data[i].accel_y, data[i].accel_z,
                    data[i].gyro_x, data[i].gyro_y, data[i].gyro_z);
            if ((i + 1) % 50 == 0) { // Yield every 50 records
                vTaskDelay(pdMS_TO_TICKS(1));
            }
        }
        vTaskDelay(pdMS_TO_TICKS(1)); // Yield before closing file
        fclose(f);
        ESP_LOGD(TAG, "Wrote %d MPU data points to %s", count, filename);
    } else {
        ESP_LOGE(TAG, "Failed to open MPU data file: %s (errno: %d - %s)", filename, errno, strerror(errno));
    }
}

// 写入温度数据到SD卡(CSV格式) - (adapted from main.c)
static void sd_module_write_temp_data() {
    uint16_t count = sd_module_swap_buffer(&temp_buffer);
    if (count == 0) return;

    char filename[64];
    sd_module_create_filename(filename, sizeof(filename), TEMP_DIR, "temp");
    bool file_exists = (access(filename, F_OK) == 0);
    FILE *f = fopen(filename, "a");
    if (f) {
        if (!file_exists) {
            fprintf(f, "timestamp,mpu_temp,ntc1_temp,ntc2_temp\n");
        }
        TempData_t *data = (TempData_t*)temp_buffer.ready_buf;
        for (int i = 0; i < count; i++) {
            struct tm timeinfo;
            char time_str[30];
            localtime_r(&data[i].timestamp.tv_sec, &timeinfo);
            strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", &timeinfo);
            long milliseconds = data[i].timestamp.tv_usec / 1000;
            fprintf(f, "%s.%03ld,%.2f,%.2f,%.2f\n", 
                    time_str, milliseconds, 
                    data[i].mpu_temp / 100.0f, 
                    data[i].ntc1_temp / 100.0f, 
                    data[i].ntc2_temp / 100.0f);
            if ((i + 1) % 50 == 0) { // Yield every 50 records
                vTaskDelay(pdMS_TO_TICKS(1));
            }
        }
        vTaskDelay(pdMS_TO_TICKS(1)); // Yield before closing file
        fclose(f);
        ESP_LOGD(TAG, "Wrote %d temperature data points to %s", count, filename);
    } else {
        ESP_LOGE(TAG, "Failed to open temperature data file: %s (errno: %d - %s)", filename, errno, strerror(errno));
    }
}

// 写入电池数据到SD卡(CSV格式) - (adapted from main.c)
static void sd_module_write_batt_data() {
    uint16_t count = sd_module_swap_buffer(&batt_buffer);
    if (count == 0) return;

    char filename[64];
    sd_module_create_filename(filename, sizeof(filename), BATT_DIR, "battery");
    bool file_exists = (access(filename, F_OK) == 0);
    FILE *f = fopen(filename, "a");
    if (f) {
        if (!file_exists) {
            fprintf(f, "timestamp,voltage_mv,soc_percent\n");
        }
        BattData_t *data = (BattData_t*)batt_buffer.ready_buf;
        for (int i = 0; i < count; i++) {
            struct tm timeinfo;
            char time_str[30];
            localtime_r(&data[i].timestamp.tv_sec, &timeinfo);
            strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", &timeinfo);
            long milliseconds = data[i].timestamp.tv_usec / 1000;
            fprintf(f, "%s.%03ld,%u,%u\n", 
                    time_str, milliseconds, 
                    data[i].voltage, 
                    data[i].soc);
            if ((i + 1) % 50 == 0) { // Yield every 50 records
                vTaskDelay(pdMS_TO_TICKS(1));
            }
        }
        vTaskDelay(pdMS_TO_TICKS(1)); // Yield before closing file
        fclose(f);
        ESP_LOGD(TAG, "Wrote %d battery data points to %s", count, filename);
    } else {
        ESP_LOGE(TAG, "Failed to open battery data file: %s (errno: %d - %s)", filename, errno, strerror(errno));
    }
}

// 数据写入任务 (adapted from main.c's data_writer_task)
static void sd_module_data_writer_task(void *arg) {
    ESP_LOGI(TAG, "SD Card Data Writer Task started. Write interval: %d seconds.", BUFFER_DURATION_SEC);

    // Initial delay to allow other initializations, including SD card mount by main
    vTaskDelay(pdMS_TO_TICKS(5000)); 

    while (1) {
        vTaskDelay(pdMS_TO_TICKS(BUFFER_DURATION_SEC * 1000));
        
        if (!sd_card_mounted()) {
            ESP_LOGW(TAG, "SD card not mounted. Skipping data write cycle.");
            // Optionally, could try to re-initialize/remount here or signal an error state
            continue;
        }

        ESP_LOGD(TAG, "Buffer status - ECG: %u, Lead: %u, MPU: %u, Temp: %u, Batt: %u", 
                 ecg_buffer.count, lead_buffer.count, mpu_buffer.count, temp_buffer.count, batt_buffer.count);

        // Ensure all necessary directories exist. sd_mkdirs is idempotent.
        // This check can be intensive if done every cycle. Consider moving to an init phase or less frequent check.
        bool dirs_ok = true;
        const char* dirs_to_check[] = {ECG_DIR, LEAD_DIR, MPU_DIR, TEMP_DIR, BATT_DIR};
        for (int i = 0; i < sizeof(dirs_to_check)/sizeof(dirs_to_check[0]); ++i) {
            struct stat st;
            if (stat(dirs_to_check[i], &st) != 0) {
                 ESP_LOGI(TAG, "Directory %s does not exist, creating.", dirs_to_check[i]);
                if (sd_mkdirs(dirs_to_check[i]) != ESP_OK) {
                    ESP_LOGE(TAG, "Failed to create directory: %s (errno: %d - %s)", dirs_to_check[i], errno, strerror(errno));
                    dirs_ok = false;
                    // break; // Optionally break if one dir creation fails
                }
            }
        }

        if (!dirs_ok) {
            ESP_LOGE(TAG, "One or more data directories could not be ensured. Skipping write cycle.");
            // Consider more robust error handling, e.g., trying to re-mount SD card
            continue;
        }
        
        // Write data from buffers
        sd_module_write_ecg_data();
        sd_module_write_lead_data();
        sd_module_write_mpu_data();
        sd_module_write_temp_data();
        sd_module_write_batt_data();
    }
}

// Public function to start background tasks for SD card operations
void sd_start_background_tasks(void) {
    // Create the data writer task
    // Stack size and priority might need adjustment based on application needs
    // Using a slightly higher priority for data writing to ensure it runs.
    xTaskCreate(sd_module_data_writer_task, "sd_writer_task", 4096, NULL, 6, &s_data_writer_task_handle);
    ESP_LOGI(TAG, "SD card data writer task created.");

    // Note: SD card hardware initialization (sd_card_init()) is expected to be called 
    // by the main application (e.g., in app_main) BEFORE calling this function.
}

// --- End of file writing functions and task ---

// --- Implementation of new getter functions for BLE data retrieval ---

int16_t sd_get_latest_ecg_raw_sample(void) {
    int16_t latest_ecg_value = 0; // Default value if no sample is available or error

    if (ecg_buffer.mutex == NULL || ecg_buffer.capacity == 0) {
        ESP_LOGE(TAG, "ECG buffer not initialized for get_latest_sample");
        return latest_ecg_value; // Or a specific error code
    }

    if (xSemaphoreTake(ecg_buffer.mutex, pdMS_TO_TICKS(10)) == pdTRUE) {
        if (ecg_buffer.count > 0) {
            // Get the last sample from the active buffer
            ECGData_t *data_ptr = (ECGData_t*)ecg_buffer.active_buf + (ecg_buffer.count - 1);
            latest_ecg_value = data_ptr->value;
        } else {
            // ESP_LOGD(TAG, "ECG buffer is empty when trying to get latest sample.");
        }
        xSemaphoreGive(ecg_buffer.mutex);
    } else {
        ESP_LOGW(TAG, "Failed to take ECG buffer mutex for get_latest_sample");
    }
    return latest_ecg_value;
}

bool sd_get_latest_other_sensors_raw_data(OtherSensorsRawDataForBLE_t *target_struct) {
    if (!target_struct) {
        return false;
    }

    // Initialize with zeros to avoid sending garbage for fields that are not ready.
    memset(target_struct, 0, sizeof(OtherSensorsRawDataForBLE_t));

    bool any_data_read = false;

    // Get latest battery data
    if (batt_buffer.mutex != NULL && xSemaphoreTake(batt_buffer.mutex, pdMS_TO_TICKS(10)) == pdTRUE) {
        if (batt_buffer.count > 0) {
            BattData_t *data_ptr = (BattData_t*)batt_buffer.active_buf + (batt_buffer.count - 1);
            target_struct->battery_voltage_mv = data_ptr->voltage;
            target_struct->battery_soc = data_ptr->soc;
            any_data_read = true;
        }
        xSemaphoreGive(batt_buffer.mutex);
    }

    // Get latest temperature data
    if (temp_buffer.mutex != NULL && xSemaphoreTake(temp_buffer.mutex, pdMS_TO_TICKS(10)) == pdTRUE) {
        if (temp_buffer.count > 0) {
            TempData_t *data_ptr = (TempData_t*)temp_buffer.active_buf + (temp_buffer.count - 1);
            target_struct->ntc1_temp_scaled = data_ptr->ntc1_temp;
            target_struct->ntc2_temp_scaled = data_ptr->ntc2_temp;
            target_struct->mpu_temp_scaled = data_ptr->mpu_temp;
            any_data_read = true;
        }
        xSemaphoreGive(temp_buffer.mutex);
    }

    // Get latest lead status data
    if (lead_buffer.mutex != NULL && xSemaphoreTake(lead_buffer.mutex, pdMS_TO_TICKS(10)) == pdTRUE) {
        if (lead_buffer.count > 0) {
            LeadData_t *data_ptr = (LeadData_t*)lead_buffer.active_buf + (lead_buffer.count - 1);
            target_struct->electrode_status = data_ptr->status;
            any_data_read = true;
        }
        xSemaphoreGive(lead_buffer.mutex);
    }

    // Get latest MPU data
    if (mpu_buffer.mutex != NULL && xSemaphoreTake(mpu_buffer.mutex, pdMS_TO_TICKS(10)) == pdTRUE) {
        if (mpu_buffer.count > 0) {
            MPUData_t *data_ptr = (MPUData_t*)mpu_buffer.active_buf + (mpu_buffer.count - 1);
            target_struct->accel_x = data_ptr->accel_x;
            target_struct->accel_y = data_ptr->accel_y;
            target_struct->accel_z = data_ptr->accel_z;
            target_struct->gyro_x = data_ptr->gyro_x;
            target_struct->gyro_y = data_ptr->gyro_y;
            target_struct->gyro_z = data_ptr->gyro_z;
            any_data_read = true;
        }
        xSemaphoreGive(mpu_buffer.mutex);
    }
    
    return any_data_read;
}

// --- End of new getter functions ---
