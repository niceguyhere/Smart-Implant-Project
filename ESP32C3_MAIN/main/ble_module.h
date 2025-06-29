#ifndef BLE_MODULE_H
#define BLE_MODULE_H

#include "esp_err.h"

/**
 * @brief 初始化BLE模块
 *
 * 该函数初始化NimBLE堆栈，配置GATT服务器，并为时间同步做准备。
 * 它应该在应用程序启动时调用一次。
 */
void ble_module_init(void);

/**
 * @brief 启动BLE广播
 *
 * 使设备可见，以便手机App可以连接。
 * 通常在 `ble_module_init` 内部的同步回调中调用。
 * 如果需要在其他时间点手动控制广播，可以公开此函数。
 * 为简单起见，目前由初始化流程自动处理。
 */
// void ble_module_start_advertising(void); // 目前由内部逻辑处理

#endif // BLE_MODULE_H
