#include "ble_module.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include <sys/time.h> // For settimeofday
#include <time.h>     // For time_t, struct tm, localtime_r, strftime
#include <errno.h>    // For errno

// NimBLE specific headers
#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "host/ble_hs.h"
#include "host/util/util.h"
#include "services/gap/ble_svc_gap.h"
#include "services/gatt/ble_svc_gatt.h"

#ifndef BLE_ADDR_STR_LEN
#define BLE_ADDR_STR_LEN (BLE_ADDR_LEN * 3) /* Max len of a BLE addr string e.g. "00:11:22:33:44:55" */
#endif

static const char *TAG = "BLE_MODULE";

// 新的特征UUID (16-bit)
#define UUID16_APP_COMMAND_CHAR      0xABCD
#define UUID16_ECG_REALTIME_CHAR     0xABCE
#define UUID16_OTHER_SENSORS_CHAR    0xABCF

// 命令字节定义
#define CMD_STOP_REALTIME            0x00
#define CMD_START_REALTIME           0x01

// 全局变量
static uint16_t g_conn_handle = BLE_HS_CONN_HANDLE_NONE; // Ensure this is managed by ble_gap_event
static bool g_realtime_stream_active = false;
static TaskHandle_t g_realtime_task_handle = NULL;
static bool g_ecg_notify_enabled = false;
static bool g_other_sensors_notify_enabled = false;

// 声明将要创建的任务
static void realtime_data_sender_task(void *param);

// Include headers for sensor data acquisition (now primarily via sd_card module)
#include "pin_control.h"
#include "sd_card.h"           // For getting latest sensor data from buffers // For SD card buffer access functions
// Note: adc_module.h, ntc_sensor.h etc. might still be needed if other parts of ble_module use them directly.
// If not, they can be removed from here.

// The OtherSensorsData_t struct is now replaced by OtherSensorsRawDataForBLE_t from sd_card.h

// --- UUIDs for Time Synchronization Service and Characteristic ---
#define UUID16_TIME_SYNC_SERVICE   0x1234
#define UUID16_CURRENT_TIME_CHAR 0x5678

// static uint16_t g_attr_handle_current_time; // This might not be used if val_handle is used directly (defined but not used)
static uint16_t g_attr_handle_ecg_realtime; // Attribute handle for ECG real-time characteristic
static uint16_t g_attr_handle_other_sensors; // Attribute handle for Other Sensors characteristic
static struct ble_hs_adv_fields g_adv_fields; // Global advertising fields

// --- Function Prototypes for BLE Event Handlers ---
static int ble_gap_event(struct ble_gap_event *event, void *arg);
static int ble_gatt_svc_access(uint16_t conn_handle, uint16_t attr_handle,
                               struct ble_gatt_access_ctxt *ctxt, void *arg);
static void ble_advertise(void); // Declaration for our advertising function

// --- GATT Service Definition ---
static const struct ble_gatt_svc_def gatt_svcs[] = {
    {
        .type = BLE_GATT_SVC_TYPE_PRIMARY,
        .uuid = BLE_UUID16_DECLARE(UUID16_TIME_SYNC_SERVICE),
        .characteristics = (struct ble_gatt_chr_def[]) {
        /*** Characteristic: Current Time */
        {
            .uuid = BLE_UUID16_DECLARE(UUID16_CURRENT_TIME_CHAR),
            .access_cb = ble_gatt_svc_access,
            .flags = BLE_GATT_CHR_F_WRITE,
        },
        /*** Characteristic: App Command */
        {
            .uuid = BLE_UUID16_DECLARE(UUID16_APP_COMMAND_CHAR),
            .access_cb = ble_gatt_svc_access,
            .flags = BLE_GATT_CHR_F_WRITE | BLE_GATT_CHR_F_WRITE_NO_RSP,
        },
        /*** Characteristic: Real-time ECG Data */
        {
            .uuid = BLE_UUID16_DECLARE(UUID16_ECG_REALTIME_CHAR),
            .access_cb = ble_gatt_svc_access,
            .flags = BLE_GATT_CHR_F_NOTIFY,
            .val_handle = &g_attr_handle_ecg_realtime, // Will be filled by NimBLE stack
        },
        /*** Characteristic: Other Sensors Data */
        {
            .uuid = BLE_UUID16_DECLARE(UUID16_OTHER_SENSORS_CHAR),
            .access_cb = ble_gatt_svc_access,
            .flags = BLE_GATT_CHR_F_NOTIFY,
            .val_handle = &g_attr_handle_other_sensors, // Will be filled by NimBLE stack
        },
        {0} /* No more characteristics in this service */
        } // End of characteristics array for the service
    }, // End of the service definition
    {0} /* Sentinel to indicate end of service array */
}; // End of gatt_svcs array definition

// --- Function to start/restart advertising ---
static void ble_advertise(void) {
    int rc;
    struct ble_gap_adv_params adv_params;

    // Set advertising parameters.
    memset(&adv_params, 0, sizeof(adv_params));
    adv_params.conn_mode = BLE_GAP_CONN_MODE_UND; // Undirected connectable.
    adv_params.disc_mode = BLE_GAP_DISC_MODE_GEN; // General discoverable.
    adv_params.itvl_min = BLE_GAP_ADV_ITVL_MS(100); // 100 ms advertising interval min
    adv_params.itvl_max = BLE_GAP_ADV_ITVL_MS(100); // 100 ms advertising interval max

    ESP_LOGI(TAG, "Starting BLE advertising...");
    // Start advertising. BLE_HS_FOREVER means advertise indefinitely.
    rc = ble_gap_adv_start(BLE_OWN_ADDR_PUBLIC, NULL /* direct_addr */, BLE_HS_FOREVER,
                           &adv_params, ble_gap_event, NULL);
    if (rc != 0) {
        ESP_LOGE(TAG, "Error starting advertising; rc=%d", rc);
        return;
    }
    // ESP_LOGI(TAG, "Advertising started successfully."); // Can be verbose, ESP_LOGD if preferred
}


// --- GAP Event Handler ---
static int ble_gap_event(struct ble_gap_event *event, void *arg) {
    // char addr_str[BLE_ADDR_STR_LEN]; // For logging BLE address - Temporarily commented out
    switch (event->type) {
    case BLE_GAP_EVENT_CONNECT:
        ESP_LOGI(TAG, "BLE_GAP_EVENT_CONNECT %s (conn_handle: 0x%x)",
                 event->connect.status == 0 ? "OK" : "Failed", event->connect.conn_handle);
        if (event->connect.status == 0) {
            g_conn_handle = event->connect.conn_handle; // Store the connection handle
            ESP_LOGI(TAG, "Connection established. conn_handle=0x%x", g_conn_handle);
            // Optionally, you might want to start advertising again if you support multiple connections
            // or handle connection parameter updates here.
        } else {
            // Connection failed, advertise again
            g_conn_handle = BLE_HS_CONN_HANDLE_NONE; // Ensure it's reset
            ble_advertise();
        }
        return 0;

    case BLE_GAP_EVENT_DISCONNECT:
        ESP_LOGI(TAG, "BLE_GAP_EVENT_DISCONNECT reason=0x%x (conn_handle: 0x%x)", 
                 event->disconnect.reason, event->disconnect.conn.conn_handle);
        
        // Check if this was the active connection handle
        if (event->disconnect.conn.conn_handle == g_conn_handle) {
            g_conn_handle = BLE_HS_CONN_HANDLE_NONE; // Reset connection handle
            ESP_LOGI(TAG, "Active connection 0x%x disconnected.", event->disconnect.conn.conn_handle);

            // Stop real-time task if it was running for this connection
            if (g_realtime_stream_active && g_realtime_task_handle != NULL) {
                ESP_LOGI(TAG, "Stopping real-time data stream task due to disconnect.");
                vTaskDelete(g_realtime_task_handle);
                g_realtime_task_handle = NULL;
            }
            g_realtime_stream_active = false;
            g_ecg_notify_enabled = false;
            g_other_sensors_notify_enabled = false;
            ESP_LOGI(TAG, "Real-time stream and notify states reset.");
        } else {
            ESP_LOGW(TAG, "Disconnected conn_handle 0x%x was not the active g_conn_handle 0x%x", 
                     event->disconnect.conn.conn_handle, g_conn_handle);
        }
        
        // Restart advertising to allow new connections
        ble_advertise();
        return 0;

    case BLE_GAP_EVENT_ADV_COMPLETE:
        ESP_LOGI(TAG, "BLE GAP EVENT_ADV_COMPLETE reason=%d", event->adv_complete.reason);
        // If advertising completes (e.g., timed out, not BLE_HS_FOREVER), restart it.
        // This might not be strictly necessary if BLE_HS_FOREVER is used and connections are handled.
        ble_advertise();
        return 0;
    
    case BLE_GAP_EVENT_SUBSCRIBE: 
        ESP_LOGI(TAG, "BLE_GAP_EVENT_SUBSCRIBE conn_handle=%d attr_handle=%d "
                      "reason=%d prev_notify=%d cur_notify=%d prev_indicate=%d cur_indicate=%d",
                      event->subscribe.conn_handle,
                      event->subscribe.attr_handle,
                      event->subscribe.reason,
                      event->subscribe.prev_notify,
                      event->subscribe.cur_notify,
                      event->subscribe.prev_indicate,
                      event->subscribe.cur_indicate);

        // Check which characteristic this subscription is for
        if (event->subscribe.attr_handle == g_attr_handle_ecg_realtime) {
            if (event->subscribe.cur_notify) {
                ESP_LOGI(TAG, "ECG Realtime notify enabled via GAP_EVENT_SUBSCRIBE (attr_handle=0x%x)", event->subscribe.attr_handle);
                g_ecg_notify_enabled = true;
            } else {
                ESP_LOGI(TAG, "ECG Realtime notify disabled via GAP_EVENT_SUBSCRIBE (attr_handle=0x%x)", event->subscribe.attr_handle);
                g_ecg_notify_enabled = false;
            }
        } else if (event->subscribe.attr_handle == g_attr_handle_other_sensors) {
            if (event->subscribe.cur_notify) {
                ESP_LOGI(TAG, "Other Sensors notify enabled via GAP_EVENT_SUBSCRIBE (attr_handle=0x%x)", event->subscribe.attr_handle);
                g_other_sensors_notify_enabled = true;
            } else {
                ESP_LOGI(TAG, "Other Sensors notify disabled via GAP_EVENT_SUBSCRIBE (attr_handle=0x%x)", event->subscribe.attr_handle);
                g_other_sensors_notify_enabled = false;
            }
        } else {
            ESP_LOGW(TAG, "Subscription event for unknown attr_handle: 0x%x", event->subscribe.attr_handle);
        }
        return 0;

    default:
        ESP_LOGD(TAG, "BLE GAP event type %d", event->type);
        return 0;
    }
}

// --- GATT Access Callback for Time Characteristic ---
static int ble_gatt_svc_access(uint16_t conn_handle_cb, uint16_t attr_handle,
                       struct ble_gatt_access_ctxt *ctxt, void *arg) {

    // <<< NEW DIAGNOSTIC LOG >>>
    if (ctxt->op == BLE_GATT_ACCESS_OP_WRITE_DSC) {
        ESP_LOGW(TAG, "[[WRITE_DSC_ENTRY]] attr_handle=0x%x, dsc_uuid=%s, chr_uuid=%s", 
                 attr_handle,
                 (ctxt->dsc && ctxt->dsc->uuid) ? ble_uuid_to_str(ctxt->dsc->uuid, (char[BLE_UUID_STR_LEN]){0}) : "N/A",
                 (ctxt->chr && ctxt->chr->uuid) ? ble_uuid_to_str(ctxt->chr->uuid, (char[BLE_UUID_STR_LEN]){0}) : "N/A");
    }
    // <<< END OF NEW DIAGNOSTIC LOG >>>

    // Basic log for all access operations
    ESP_LOGI(TAG, "ble_gatt_svc_access: op=%d (0x%x), attr_handle=0x%x, conn_handle=0x%x", 
             ctxt->op, ctxt->op, attr_handle, conn_handle_cb);

    // Detailed log for characteristic or descriptor access
    if (ctxt->op == BLE_GATT_ACCESS_OP_READ_CHR || ctxt->op == BLE_GATT_ACCESS_OP_WRITE_CHR) {
        if (ctxt->chr != NULL) { // Add null check for safety
            ESP_LOGI(TAG, "Accessing CHR UUID: %s", ble_uuid_to_str(ctxt->chr->uuid, (char[BLE_UUID_STR_LEN]){0}));
        }
    } else if (ctxt->op == BLE_GATT_ACCESS_OP_READ_DSC || ctxt->op == BLE_GATT_ACCESS_OP_WRITE_DSC) {
        if (ctxt->dsc != NULL && ctxt->chr != NULL) { // Add null checks for safety
            ESP_LOGI(TAG, "Accessing DSC UUID: %s on CHR UUID: %s (global attr_handle for this DSC op: 0x%x)",
                     ble_uuid_to_str(ctxt->dsc->uuid, (char[BLE_UUID_STR_LEN]){0}),
                     ble_uuid_to_str(ctxt->chr->uuid, (char[BLE_UUID_STR_LEN]){0}),
                     attr_handle); // attr_handle from function args is the global handle for this descriptor
        }
    }

    // ESP_LOGI(TAG, "GATT Access: op=%d, attr_handle=%d, chr_uuid=%s", ctxt->op, attr_handle, ble_uuid_to_str(ctxt->chr->uuid, buf));

    if (ctxt->op == BLE_GATT_ACCESS_OP_WRITE_DSC) {
        if (ble_uuid_cmp(ctxt->dsc->uuid, BLE_UUID16_DECLARE(BLE_GATT_DSC_CLT_CFG_UUID16)) == 0) {
            uint16_t value_le;
            uint16_t value;
            if (OS_MBUF_PKTLEN(ctxt->om) == 2) {
                ble_hs_mbuf_to_flat(ctxt->om, &value_le, sizeof(value_le), NULL);
                value = le16toh(value_le); // Convert little-endian uint16_t to host byte order

                if (ble_uuid_cmp(ctxt->chr->uuid, BLE_UUID16_DECLARE(UUID16_ECG_REALTIME_CHAR)) == 0) {
                    if (value & BLE_GATT_CHR_F_NOTIFY) { // Check if NOTIFY bit is set
                        ESP_LOGI(TAG, "ECG Realtime notify enabled by client");
                        g_ecg_notify_enabled = true;
                    } else {
                        ESP_LOGI(TAG, "ECG Realtime notify disabled by client");
                        g_ecg_notify_enabled = false;
                    }
                    return 0; // Success
                } else if (ble_uuid_cmp(ctxt->chr->uuid, BLE_UUID16_DECLARE(UUID16_OTHER_SENSORS_CHAR)) == 0) {
                    if (value & BLE_GATT_CHR_F_NOTIFY) { // Check if NOTIFY bit is set
                        ESP_LOGI(TAG, "Other Sensors notify enabled by client");
                        g_other_sensors_notify_enabled = true;
                    } else {
                        ESP_LOGI(TAG, "Other Sensors notify disabled by client");
                        g_other_sensors_notify_enabled = false;
                    }
                    return 0; // Success
                }
            }
        }
        return BLE_ATT_ERR_UNLIKELY; // Should not happen for other descriptors if not defined

    } else if (ctxt->op == BLE_GATT_ACCESS_OP_WRITE_CHR) {
        if (ble_uuid_cmp(ctxt->chr->uuid, BLE_UUID16_DECLARE(UUID16_CURRENT_TIME_CHAR)) == 0) {
            ESP_LOGI(TAG, "Write to Current Time Characteristic");
            if (OS_MBUF_PKTLEN(ctxt->om) == sizeof(uint64_t)) {
                uint64_t time_ms_le;
                int rc = ble_hs_mbuf_to_flat(ctxt->om, &time_ms_le, sizeof(time_ms_le), NULL);
                if (rc == 0) {
                    uint64_t time_ms = le64toh(time_ms_le); // Assuming ESP32 is little-endian, direct use might be okay, but le64toh is safer for portability
                    struct timeval tv;
                    tv.tv_sec = time_ms / 1000;
                    tv.tv_usec = (time_ms % 1000) * 1000;
                    if (settimeofday(&tv, NULL) == 0) {
                        ESP_LOGI(TAG, "System time updated successfully via BLE: %llu ms", time_ms);
                        time_t now;
                        struct tm timeinfo_tm;
                        time(&now);
                        localtime_r(&now, &timeinfo_tm);
                        ESP_LOGI(TAG, "Current system time: %s", asctime(&timeinfo_tm));
                    } else {
                        ESP_LOGE(TAG, "Failed to set system time. errno: %d (%s)", errno, strerror(errno));
                        return BLE_ATT_ERR_UNLIKELY;
                    }
                } else {
                    ESP_LOGE(TAG, "Failed to flatten mbuf for time data");
                    return BLE_ATT_ERR_INVALID_PDU;
                }
            } else {
                ESP_LOGE(TAG, "Invalid length for time data: %d bytes, expected %d", OS_MBUF_PKTLEN(ctxt->om), (int)sizeof(uint64_t));
                return BLE_ATT_ERR_INVALID_PDU;
            }
            return 0; // Success
        } else if (ble_uuid_cmp(ctxt->chr->uuid, BLE_UUID16_DECLARE(UUID16_APP_COMMAND_CHAR)) == 0) {
            uint8_t command;
            if (OS_MBUF_PKTLEN(ctxt->om) == 1) {
                ble_hs_mbuf_to_flat(ctxt->om, &command, sizeof(command), NULL);
                ESP_LOGI(TAG, "App Command received: 0x%02X", command);
                if (command == CMD_START_REALTIME) {
                    if (!g_realtime_stream_active && g_conn_handle != BLE_HS_CONN_HANDLE_NONE) {
                        ESP_LOGI(TAG, "Starting real-time data stream task");
                        g_realtime_stream_active = true;
                        BaseType_t rtos_rslt = xTaskCreate(realtime_data_sender_task, "rt_data_task", 4096, NULL, 5, &g_realtime_task_handle);
                        if (rtos_rslt != pdPASS) {
                            ESP_LOGE(TAG, "Failed to create real-time task");
                            g_realtime_stream_active = false; // Revert state
                            return BLE_ATT_ERR_INSUFFICIENT_RES;
                        }
                    } else if (g_realtime_stream_active) {
                        ESP_LOGI(TAG, "Real-time stream already active.");
                    } else if (g_conn_handle == BLE_HS_CONN_HANDLE_NONE) {
                        ESP_LOGW(TAG, "Cannot start real-time stream: No active connection.");
                        return BLE_ATT_ERR_WRITE_NOT_PERMITTED; // Or other suitable error
                    }
                } else if (command == CMD_STOP_REALTIME) {
                    if (g_realtime_stream_active && g_realtime_task_handle != NULL) {
                        ESP_LOGI(TAG, "Stopping real-time data stream task");
                        vTaskDelete(g_realtime_task_handle);
                        g_realtime_task_handle = NULL;
                    }
                    g_realtime_stream_active = false; // Ensure it's set to false
                    ESP_LOGI(TAG, "Real-time stream stopped.");
                } else {
                    ESP_LOGW(TAG, "Unknown app command: 0x%02X", command);
                    return BLE_ATT_ERR_INVALID_PDU;
                }
                return 0; // Success
            } else {
                ESP_LOGE(TAG, "Invalid length for App Command: %d, expected 1", OS_MBUF_PKTLEN(ctxt->om));
                return BLE_ATT_ERR_INVALID_ATTR_VALUE_LEN;
            }
        } else {
            // Attempting to write to a characteristic not handled by this logic for writes
            ESP_LOGW(TAG, "Write attempt to unhandled characteristic UUID in ble_gatt_svc_access: %s", ble_uuid_to_str(ctxt->chr->uuid, (char[BLE_UUID_STR_LEN]){0}));
            return BLE_ATT_ERR_ATTR_NOT_FOUND;
        }
    } // End of BLE_GATT_ACCESS_OP_WRITE_CHR
    // else if (ctxt->op == BLE_GATT_ACCESS_OP_READ_CHR) { /* Handle read if necessary */ }

    // If the operation is not a handled write (DSC or CHR) or a handled read, it's not supported by this callback.
    ESP_LOGW(TAG, "Unhandled GATT Access operation: op=%d for attr_handle=%d", ctxt->op, attr_handle);
    return BLE_ATT_ERR_REQ_NOT_SUPPORTED;
}

// --- BLE Initialization Functions ---
static void ble_on_reset(int reason) {
    MODLOG_DFLT(ERROR, "Resetting state; reason=%d\n", reason);
}

static void ble_on_sync(void) {
    int rc;

    rc = ble_hs_util_ensure_addr(0); // Ensure public address is available
    assert(rc == 0);

    // Set device name
    rc = ble_svc_gap_device_name_set("ESP32C3-TimeSync");
    assert(rc == 0);

    // Configure advertising fields using the global structure
    memset(&g_adv_fields, 0, sizeof(g_adv_fields));
    g_adv_fields.flags = BLE_HS_ADV_F_DISC_GEN | BLE_HS_ADV_F_BREDR_UNSUP;
    g_adv_fields.tx_pwr_lvl_is_present = 1;
    g_adv_fields.tx_pwr_lvl = BLE_HS_ADV_TX_PWR_LVL_AUTO; // Or specific value like 0 for 0dBm
    
    const char *name = ble_svc_gap_device_name();
    g_adv_fields.name = (uint8_t *)name;
    g_adv_fields.name_len = strlen(name);
    g_adv_fields.name_is_complete = 1;

    // Add service UUID to advertisement data if space permits
    // g_adv_fields.uuids16 = (ble_uuid16_t[]) {BLE_UUID16_INIT(UUID16_TIME_SYNC_SERVICE)};
    // g_adv_fields.num_uuids16 = 1;
    // g_adv_fields.uuids16_is_complete = 1;

    rc = ble_gap_adv_set_fields(&g_adv_fields); // Set advertising data
    if (rc != 0) {
        ESP_LOGE(TAG, "Error setting advertising fields; rc=%d", rc);
        return;
    }

    // Start advertising
    ble_advertise();
}

void nimble_host_task(void *param) {
    ESP_LOGI(TAG, "BLE Host Task Started");
    nimble_port_run(); 
    nimble_port_freertos_deinit();
}

// Define frequencies for sending data
#define ECG_SEND_INTERVAL_MS (1000 / 50)  // 50Hz -> 20ms
#define OTHER_SENSORS_SEND_INTERVAL_MS (5000) // 0.2Hz (5 seconds)

static void realtime_data_sender_task(void *param)
{
    TickType_t xLastWakeTime;
    TickType_t last_other_sensors_send_ticks;
    const TickType_t xFrequencyECG = pdMS_TO_TICKS(ECG_SEND_INTERVAL_MS);

    xLastWakeTime = xTaskGetTickCount();
    last_other_sensors_send_ticks = xLastWakeTime;

    ESP_LOGI(TAG, "Real-time data sender task started.");

    while (g_realtime_stream_active && g_conn_handle != BLE_HS_CONN_HANDLE_NONE)
    {
        vTaskDelayUntil(&xLastWakeTime, xFrequencyECG);

        // --- Send ECG Data ---
        if (g_ecg_notify_enabled) {
            int16_t ecg_sample = sd_get_latest_ecg_raw_sample();
            struct os_mbuf *om = ble_hs_mbuf_from_flat(&ecg_sample, sizeof(ecg_sample));
            if (om) {
                int rc = ble_gatts_notify_custom(g_conn_handle, g_attr_handle_ecg_realtime, om);
                if (rc != 0) {
                    ESP_LOGW(TAG, "ECG notify failed, rc=%d", rc);
                }
            } else {
                ESP_LOGE(TAG, "ECG mbuf alloc failed");
            }
        }

        // --- Send Other Sensors Data ---
        TickType_t current_ticks = xTaskGetTickCount();
        if (g_other_sensors_notify_enabled && (current_ticks - last_other_sensors_send_ticks >= pdMS_TO_TICKS(OTHER_SENSORS_SEND_INTERVAL_MS))) {
            ESP_LOGI(TAG, "Attempting to send Other Sensors. Notify_enabled: %d, Stream_active: %d, Conn_handle: 0x%x", g_other_sensors_notify_enabled, g_realtime_stream_active, g_conn_handle);
            // Update timestamp regardless of success to maintain interval
            last_other_sensors_send_ticks = current_ticks;

            OtherSensorsRawDataForBLE_t other_data_ble;
            if (sd_get_latest_other_sensors_raw_data(&other_data_ble)) {
                ESP_LOGI(TAG, "OtherSensors data for BLE: NTC1=%d, NTC2=%d, MPU_T=%d, Elec=%u, SOC=%u, V_mV=%u, AccX=%d",
                        other_data_ble.ntc1_temp_scaled, other_data_ble.ntc2_temp_scaled, other_data_ble.mpu_temp_scaled,
                        other_data_ble.electrode_status, other_data_ble.battery_soc,
                        other_data_ble.battery_voltage_mv, other_data_ble.accel_x);

                struct os_mbuf *om = ble_hs_mbuf_from_flat(&other_data_ble, sizeof(other_data_ble));
                if (om) {
                    int rc = ble_gatts_notify_custom(g_conn_handle, g_attr_handle_other_sensors, om);
                    ESP_LOGI(TAG, "ble_gatts_notify_custom for OtherSensors (handle 0x%x) returned: %d (0x%x)", g_attr_handle_other_sensors, rc, rc);
                } else {
                    ESP_LOGE(TAG, "OtherSensors mbuf alloc (from_flat) failed");
                }
            } else {
                ESP_LOGW(TAG, "Failed to get latest other sensors data for BLE notification.");
            }
        }
        
        // If no notifications are enabled, delay longer to save CPU.
        if (!g_ecg_notify_enabled && !g_other_sensors_notify_enabled) {
            vTaskDelay(pdMS_TO_TICKS(100)); 
            xLastWakeTime = xTaskGetTickCount(); // Reset wake time after a variable delay
        }
    } // end while

    ESP_LOGI(TAG, "Real-time data sender task exiting. Reason: stream_active=%d, conn_handle=0x%x",
             g_realtime_stream_active, g_conn_handle);
    g_realtime_task_handle = NULL;
    vTaskDelete(NULL);
} // End of realtime_data_sender_task

void ble_module_init(void) {
    int rc;

    ESP_LOGI(TAG, "Initializing BLE module...");

    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    nimble_port_init();

    ble_hs_cfg.reset_cb = ble_on_reset;
    ble_hs_cfg.sync_cb = ble_on_sync;
    ble_hs_cfg.gatts_register_cb = NULL; 
    ble_hs_cfg.store_status_cb = ble_store_util_status_rr; 

    rc = ble_gatts_count_cfg(gatt_svcs);
    assert(rc == 0);
    rc = ble_gatts_add_svcs(gatt_svcs);
    assert(rc == 0);

    nimble_port_freertos_init(nimble_host_task);

    ESP_LOGI(TAG, "BLE module initialization complete.");
}
