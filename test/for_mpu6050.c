#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/i2c.h"
#include "esp_log.h"
#include "esp_err.h"

static const char *TAG = "MPU6050";

// I2C configuration
#define I2C_MASTER_SCL_IO           GPIO_NUM_8     // SCL pin
#define I2C_MASTER_SDA_IO           GPIO_NUM_9     // SDA pin
#define I2C_MASTER_NUM              I2C_NUM_0      // I2C port number
#define I2C_MASTER_FREQ_HZ          400000         // I2C master clock frequency
#define I2C_MASTER_TX_BUF_DISABLE   0              // I2C master doesn't need buffer
#define I2C_MASTER_RX_BUF_DISABLE   0              // I2C master doesn't need buffer
#define I2C_MASTER_TIMEOUT_MS       1000

// MPU6050 configuration
#define MPU6050_ADDR                0x68           // MPU6050 I2C address (AD0 = 0)
#define MPU6050_WHO_AM_I_REG        0x75
#define MPU6050_PWR_MGMT_1_REG      0x6B
#define MPU6050_SMPLRT_DIV_REG      0x19
#define MPU6050_CONFIG_REG          0x1A
#define MPU6050_GYRO_CONFIG_REG     0x1B
#define MPU6050_ACCEL_CONFIG_REG    0x1C
#define MPU6050_ACCEL_XOUT_H_REG    0x3B
#define MPU6050_TEMP_OUT_H_REG      0x41
#define MPU6050_GYRO_XOUT_H_REG     0x43

// Data structure for MPU6050 data
typedef struct {
    int16_t accel_x;
    int16_t accel_y;
    int16_t accel_z;
    int16_t temp;
    int16_t gyro_x;
    int16_t gyro_y;
    int16_t gyro_z;
} mpu6050_data_t;

// I2C master initialization
static esp_err_t i2c_master_init(void)
{
    i2c_config_t conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = I2C_MASTER_SDA_IO,
        .scl_io_num = I2C_MASTER_SCL_IO,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = I2C_MASTER_FREQ_HZ,
    };
    
    esp_err_t err = i2c_param_config(I2C_MASTER_NUM, &conf);
    if (err != ESP_OK) {
        return err;
    }
    
    return i2c_driver_install(I2C_MASTER_NUM, conf.mode, I2C_MASTER_RX_BUF_DISABLE, I2C_MASTER_TX_BUF_DISABLE, 0);
}

// Write a byte to MPU6050 register
static esp_err_t mpu6050_write_byte(uint8_t reg_addr, uint8_t data)
{
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (MPU6050_ADDR << 1) | I2C_MASTER_WRITE, true);
    i2c_master_write_byte(cmd, reg_addr, true);
    i2c_master_write_byte(cmd, data, true);
    i2c_master_stop(cmd);
    
    esp_err_t ret = i2c_master_cmd_begin(I2C_MASTER_NUM, cmd, I2C_MASTER_TIMEOUT_MS / portTICK_PERIOD_MS);
    i2c_cmd_link_delete(cmd);
    
    return ret;
}

// Read bytes from MPU6050 register
static esp_err_t mpu6050_read_bytes(uint8_t reg_addr, uint8_t *data, size_t len)
{
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    
    // Write register address
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (MPU6050_ADDR << 1) | I2C_MASTER_WRITE, true);
    i2c_master_write_byte(cmd, reg_addr, true);
    
    // Read data
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (MPU6050_ADDR << 1) | I2C_MASTER_READ, true);
    if (len > 1) {
        i2c_master_read(cmd, data, len - 1, I2C_MASTER_ACK);
    }
    i2c_master_read_byte(cmd, data + len - 1, I2C_MASTER_NACK);
    i2c_master_stop(cmd);
    
    esp_err_t ret = i2c_master_cmd_begin(I2C_MASTER_NUM, cmd, I2C_MASTER_TIMEOUT_MS / portTICK_PERIOD_MS);
    i2c_cmd_link_delete(cmd);
    
    return ret;
}

// Initialize MPU6050
static esp_err_t mpu6050_init(void)
{
    esp_err_t ret;
    uint8_t who_am_i;
    
    // Check WHO_AM_I register
    ret = mpu6050_read_bytes(MPU6050_WHO_AM_I_REG, &who_am_i, 1);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to read WHO_AM_I register");
        return ret;
    }
    
    ESP_LOGI(TAG, "WHO_AM_I: 0x%02X", who_am_i);
    if (who_am_i != 0x68) {
        ESP_LOGE(TAG, "MPU6050 not found! Expected 0x68, got 0x%02X", who_am_i);
        return ESP_ERR_NOT_FOUND;
    }
    
    // Wake up MPU6050 (exit sleep mode)
    ret = mpu6050_write_byte(MPU6050_PWR_MGMT_1_REG, 0x00);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to wake up MPU6050");
        return ret;
    }
    
    // Set sample rate divider for 10Hz (1kHz / (99 + 1) = 10Hz)
    ret = mpu6050_write_byte(MPU6050_SMPLRT_DIV_REG, 99);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set sample rate");
        return ret;
    }
    
    // Set digital low pass filter
    ret = mpu6050_write_byte(MPU6050_CONFIG_REG, 0x06);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set config register");
        return ret;
    }
    
    // Set gyroscope full scale range to ±250°/s
    ret = mpu6050_write_byte(MPU6050_GYRO_CONFIG_REG, 0x00);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set gyroscope config");
        return ret;
    }
    
    // Set accelerometer full scale range to ±2g
    ret = mpu6050_write_byte(MPU6050_ACCEL_CONFIG_REG, 0x00);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set accelerometer config");
        return ret;
    }
    
    ESP_LOGI(TAG, "MPU6050 initialized successfully");
    return ESP_OK;
}

// Read all sensor data from MPU6050
static esp_err_t mpu6050_read_all_data(mpu6050_data_t *data)
{
    uint8_t raw_data[14];
    
    // Read all 14 bytes from ACCEL_XOUT_H to GYRO_ZOUT_L
    esp_err_t ret = mpu6050_read_bytes(MPU6050_ACCEL_XOUT_H_REG, raw_data, 14);
    if (ret != ESP_OK) {
        return ret;
    }
    
    // Convert raw data to 16-bit signed integers
    data->accel_x = (int16_t)((raw_data[0] << 8) | raw_data[1]);
    data->accel_y = (int16_t)((raw_data[2] << 8) | raw_data[3]);
    data->accel_z = (int16_t)((raw_data[4] << 8) | raw_data[5]);
    data->temp = (int16_t)((raw_data[6] << 8) | raw_data[7]);
    data->gyro_x = (int16_t)((raw_data[8] << 8) | raw_data[9]);
    data->gyro_y = (int16_t)((raw_data[10] << 8) | raw_data[11]);
    data->gyro_z = (int16_t)((raw_data[12] << 8) | raw_data[13]);
    
    return ESP_OK;
}

// Convert raw data to physical units
static void convert_to_physical_units(const mpu6050_data_t *raw_data, 
                                    float *accel_g, float *gyro_dps, float *temp_c)
{
    // Convert accelerometer data to g (±2g range, 16-bit)
    accel_g[0] = (float)raw_data->accel_x / 16384.0f;
    accel_g[1] = (float)raw_data->accel_y / 16384.0f;
    accel_g[2] = (float)raw_data->accel_z / 16384.0f;
    
    // Convert gyroscope data
