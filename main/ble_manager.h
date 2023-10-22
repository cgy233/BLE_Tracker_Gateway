#ifndef __BLE_MANAGER_H__
#define __BLE_MANAGER_H__

#include "msg_list.h"

#define TAG "SPUER_GW"

#define REMOTE_SERVICE_UUID 0xFFF0
#define REMOTE_NOTIFY_CHAR_UUID 0xFFF6
#define PROFILE_NUM 1
#define PROFILE_A_APP_ID 0
#define INVALID_HANDLE 0
#define MAX_SCAN_TIMES 8
#define BLE_NAME_LEN 14
#define BLE_DOG_TIME 12
//---------------------------------------
typedef unsigned char byte;

// MQTT QOS
#define QOS0 0
#define QOS1 1
#define QOS2 2

//MQTT CMD
#define XV_GW_UPDATE_TIME 0
#define XV_LOCK_LIST_LENGTH 8 // Lock count

#define DEFAULT_VREF    1100        //Use adc2_vref_to_gpio() to obtain a better estimate
#define NO_OF_SAMPLES   64          //Multisampling

// 定义最大设备数量
#define MAX_DEVICES 10

// struct

// 结构体定义：ble_device_t
struct ble_device_t {
    char name[20];    // 字符串数组，用于存储设备名称，最大长度为20个字符（包括终止符）。
	uint8_t mac_address[6];
    uint8_t confidence;   // 整数类型，用于存储设备信任度或置信度。
	int8_t rssi;
};

// 全局结构体：ble_devices_state_t
struct ble_devices_state_t {
    int num_devices; // 存储当前蓝牙设备的数量
    struct ble_device_t devices[MAX_DEVICES]; // 使用固定大小的数组来存储多个蓝牙设备。
};

// Lock list Attribute
typedef struct {
	char	        sn[15];
    int             rssi;
    bool            online;
    bool            has_data;
    bool            checked;
    uint8_t         no_checked;

    esp_ble_gap_cb_param_t dev_cb;
    bool            set_dev;
}ble_device;

struct gattc_profile_inst
{
    esp_gattc_cb_t gattc_cb;
    uint16_t gattc_if;
    uint16_t app_id;
    uint16_t conn_id;
    uint16_t service_start_handle;
    uint16_t service_end_handle;
    uint16_t char_handle;
    esp_bd_addr_t remote_bda;
};

// func
void init_ble_devices_state(struct ble_devices_state_t* state);
void check_lock_data();
void ble_device_init(ble_device *device, char *sn);
void ble_devices_init();
void ble_device_set_dev(ble_device *device, esp_ble_gap_cb_param_t *dev);
void ble_empty_cmd_data();
void BLE_init();
void ble_scan();
bool ble_device_check(ble_device *device, uint8_t *sn);
int add_ble_device(struct ble_devices_state_t* state, const char* name, const uint8_t* mac_address, int confidence);

/* Declare static functions */
void esp_gap_cb(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param);
void esp_gattc_cb(esp_gattc_cb_event_t event, esp_gatt_if_t gattc_if, esp_ble_gattc_cb_param_t *param);
void gattc_profile_event_handler(esp_gattc_cb_event_t event, esp_gatt_if_t gattc_if, esp_ble_gattc_cb_param_t *param);

#endif