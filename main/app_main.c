/*
 * @Author: cgy233 1781387847@qq.com
 * @Date: 2022-04-26 01:30:40
 * @LastEditors: error: git config user.name && git config user.email & please set dead value or install git
 * @LastEditTime: 2022-06-12 17:56:41
 * @FilePath: \BLE_Tracker_Gateway\main\app_main.c
 * @Description: 
 * 
 * Copyright (c) 2022 by cgy233 1781387847@qq.com, All Rights Reserved. 
 */
/*
   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#include "nvs.h"
#include "nvs_flash.h"

#include <sys/time.h>
#include <time.h>

#include "esp_bt.h"
#include "esp32/rom/ets_sys.h"
#include "esp_gap_ble_api.h"
#include "esp_gattc_api.h"
#include "esp_gatt_defs.h"
#include "esp_bt_main.h"
#include "esp_gatt_common_api.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_event.h"
#include "esp_task_wdt.h"
#include "driver/adc.h"
#include "esp_adc_cal.h"

#include "esp_http_client.h"
#include "esp_flash_partitions.h"
#include "esp_partition.h"

#include "lwip/sockets.h"
#include "lwip/dns.h"
#include "lwip/netdb.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "freertos/queue.h"
#include "freertos/timers.h"
#include "freertos/event_groups.h"

#include "mqtt_client.h"
#include "mbedtls/aes.h"

#include "soc/rtc_wdt.h"
#include "msg_list.h"
#include <math.h>
#include "json.h"
#include "driver/gpio.h"
#include "errno.h"
#include "ssd1306.h"

#include "led.h"
#include "button.h"
#include "wifi_smartconfig.h"
#include "ota.h"
#include "ethernet.h"
#include "ping_baidu.h"
#include "display.h"
#include "air_conditioner.h"

#include "sntp_tools.h"


#define TAG "XV_SPUER_GW"
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

static esp_adc_cal_characteristics_t *adc_chars;
static const adc_channel_t channel = ADC_CHANNEL_6;     //GPIO34 if ADC1, GPIO14 if ADC2
static const adc_bits_width_t width = ADC_WIDTH_BIT_12;
static const adc_atten_t atten = ADC_ATTEN_DB_6;
static const adc_unit_t unit = ADC_UNIT_1;
// MQTT Parameter
#define HOST_NAME "bemfa.com" 
#define HOST_PORT 9501
#define CLIENT_ID "2f13e215aec14d059745eb027aea6d47" 
#define AIR_CMD 14 // Lock count
#define TOPIC "Air005"

SSD1306_t dev;

int g_mi_band_rssi = 0;

uint8_t g_version; // Firmware version

uint8_t g_led_flag = 0; // led status flag
uint8_t  ethernet_connect_ok = 0; // led status flag

bool g_ble_scan_param_complete = false;
bool g_ble_scaning = false;
uint8_t g_ble_scan_count = 0;
short g_scan_max = 10;
bool ble_is_connected = false;

char g_buffer_list[MSG_MAX_COUNT][MSG_MAX_BUFFER];
char g_buffer_set[MSG_MAX_BUFFER] = {0};
char g_buffer_cmd[MSG_MAX_BUFFER] = {0};
static int g_msg_last = 0;

esp_mqtt_client_handle_t g_mqtt_client; // MQTT client handle

char g_topic_up[32] = "esp32";

uint8_t g_pin[20] = {0x7f, 0x12, 0x0c, 0x8f, 0x92, 0x40, 0xb8, 0xc4, 0x53, 0x07, 0x3b, 0x42, 0x31, 0x51, 0xa8, 0x45, 0xdc, 0x7a, 0xfb, 0xbd}; // BLE Ping data
uint8_t g_buf_send[32] = {0}; // BLE send data
byte g_static_key[16] = {0xca, 0x5b, 0xd3, 0x8a, 0xe8, 0x59, 0x73, 0xfd, 0x77, 0x59, 0xec, 0x02, 0x00, 0x00, 0x00, 0x00}; // BLE Secret Key
byte g_dynamic_key[4] = {0}; // BLE dynamic key
static uint32_t duration = 15; // Scanning period

msg_base *g_msg_cmd = 0;

char rssi_str[5];
uint8_t eco_flag = 0;
uint8_t sleep_flag = 0;

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

/**
 * @description: Initializes the data state of the lock
 * @param {ble_device} *device
 * @param {char} *sn
 * @return {*}
 */
void ble_device_init(ble_device *device, char *sn)
{
    memset(device->sn, 0, 15);
    memcpy(device->sn, sn, BLE_NAME_LEN);
    memset((char *)&device->dev_cb, 0 , sizeof(esp_ble_gap_cb_param_t));
    device->set_dev = false;
    device->has_data = false;
    device->no_checked = 0;
    device->rssi = 0;
    device->checked = false;
}
void ble_device_set_dev(ble_device *device, esp_ble_gap_cb_param_t *dev)
{
    memcpy((char *)&device->dev_cb, (char *)dev, sizeof(esp_ble_gap_cb_param_t));
    device->set_dev = true;
    device->no_checked = 0;
}
bool ble_device_check(ble_device *device, uint8_t *sn)
{
    return (memcmp(device->sn, sn, BLE_NAME_LEN) == 0);
}

ble_device g_device_list[XV_LOCK_LIST_LENGTH];
static int g_current_decvice = -1;

static bool get_server = false;
static esp_gattc_char_elem_t *char_elem_result = NULL;
static esp_gattc_descr_elem_t *descr_elem_result = NULL;

/* Declare static functions */
static void esp_gap_cb(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param);
static void esp_gattc_cb(esp_gattc_cb_event_t event, esp_gatt_if_t gattc_if, esp_ble_gattc_cb_param_t *param);
static void gattc_profile_event_handler(esp_gattc_cb_event_t event, esp_gatt_if_t gattc_if, esp_ble_gattc_cb_param_t *param);

static esp_bt_uuid_t remote_filter_service_uuid = {
    .len = ESP_UUID_LEN_16,
    .uuid = {
        .uuid16 = REMOTE_SERVICE_UUID,
    },
};

static esp_bt_uuid_t remote_filter_char_uuid = {
    .len = ESP_UUID_LEN_16,
    .uuid = {
        .uuid16 = REMOTE_NOTIFY_CHAR_UUID,
    },
};

static esp_bt_uuid_t notify_descr_uuid = {
    .len = ESP_UUID_LEN_16,
    .uuid = {
        .uuid16 = ESP_GATT_UUID_CHAR_CLIENT_CONFIG,
    },
};

static esp_ble_scan_params_t ble_scan_params = {
    .scan_type = BLE_SCAN_TYPE_ACTIVE,
    .own_addr_type = BLE_ADDR_TYPE_PUBLIC,
    // .scan_filter_policy = BLE_SCAN_FILTER_ALLOW_ALL,
    .scan_filter_policy = BLE_SCAN_FILTER_ALLOW_ONLY_WLST,
    .scan_interval = 0x50,
    .scan_window = 0x30,
    .scan_duplicate = BLE_SCAN_DUPLICATE_DISABLE};

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

/* One gatt-based profile one app_id and one gattc_if, this array will store the gattc_if returned by ESP_GATTS_REG_EVT */
static struct gattc_profile_inst gl_profile_tab[PROFILE_NUM] = {
    [PROFILE_A_APP_ID] = {
        .gattc_cb = gattc_profile_event_handler,
        .gattc_if = ESP_GATT_IF_NONE, /* Not get the gatt_if, so initial is ESP_GATT_IF_NONE */
    },
};

AC_INFO ac_current_info = {
    .on = false,
    .mode = AUTO_MODE,
    .temp = 16,
    .fan_speed = AUTO_FAN_SPEED
};

/**
 * @description: Clear global control command
 * @param {*}
 * @return {*}
 */
void ble_empty_cmd_data()
{
    if(0 != g_msg_cmd){
        memset((char *)g_msg_cmd, 0, MSG_MAX_BUFFER);
    }
    char *pt = (char *)g_msg_cmd;
    // ESP_LOGI(TAG, "ble_empty_cmd_data %d", (int)pt);
    ESP_LOGI(TAG, "ble_empty_cmd_data %d", (int)pt);
    g_msg_cmd = 0;
}
/**
 * @description: Put the instructions you receive into the queue
 * @param {msg_base} *msg
 * @return {*}
 */
void msg_set(msg_base *msg)
{
    g_scan_max = 3;
	for(int i = 0; i < MSG_MAX_COUNT; i ++)
	{
        char *data = g_buffer_list[i];
        data += 1;
		if(0 == *data)
		{
            ///ESP_LOGI(TAG, "22222222 %d, %s", i, data+1);
			memcpy(g_buffer_list[i], (char *)msg, MSG_MAX_BUFFER);
			return;
		}
        else{
            //ESP_LOGI(TAG, "yyyyyyy %d- %s", i, data);
        }
	}
    //ESP_LOGI(TAG, "rrrrrr %d", g_msg_last);
	memcpy(g_buffer_list[g_msg_last], (char *)msg, MSG_MAX_BUFFER);
	++ g_msg_last;
	if(g_msg_last >= MSG_MAX_COUNT){
		g_msg_last = 0;
	}
}
/**
 * @description: Remove the command from the queue
 * @param {*}
 * @return {*}
 */
msg_base* msg_get()
{
	for(int i = 0; i < MSG_MAX_COUNT; i ++)
	{
        char *data = g_buffer_list[i];
        data += 1;
        //ESP_LOGI(TAG, "msg_get %d, %s", i, data);
		if(*data != 0){
			return (msg_base * )g_buffer_list[i];
		}
	}
	return 0;
}

static void print_char_val_type(esp_adc_cal_value_t val_type)
{
    if (val_type == ESP_ADC_CAL_VAL_EFUSE_TP) {
        printf("Characterized using Two Point Value\n");
    } else if (val_type == ESP_ADC_CAL_VAL_EFUSE_VREF) {
        printf("Characterized using eFuse Vref\n");
    } else {
        printf("Characterized using Default Vref\n");
    }
}

void soid_humidity()
{
	    //Configure ADC
    if (unit == ADC_UNIT_1) {
        adc1_config_width(width);
        adc1_config_channel_atten(channel, atten);
    } else {
        adc2_config_channel_atten((adc2_channel_t)channel, atten);
    }

    //Characterize ADC
    adc_chars = calloc(1, sizeof(esp_adc_cal_characteristics_t));
    esp_adc_cal_value_t val_type = esp_adc_cal_characterize(unit, atten, width, DEFAULT_VREF, adc_chars);
    print_char_val_type(val_type);

    //Continuously sample ADC1
    while (1) {
        uint32_t adc_reading = 0;
        //Multisampling
        for (int i = 0; i < NO_OF_SAMPLES; i++) {
            if (unit == ADC_UNIT_1) {
                adc_reading += adc1_get_raw((adc1_channel_t)channel);
            } else {
                int raw;
                adc2_get_raw((adc2_channel_t)channel, width, &raw);
                adc_reading += raw;
            }
        }
        adc_reading /= NO_OF_SAMPLES;
        //Convert adc_reading to voltage in mV
        uint32_t voltage = esp_adc_cal_raw_to_voltage(adc_reading, adc_chars);
        printf("Raw: %d\tVoltage: %dmV\n", adc_reading, voltage);
		if(json_start())
		{
			json_put_int("raw", adc_reading);
			json_split();
			json_put_int("voltage", voltage);
			
			json_end();
			char *buffer_ = json_buffer();
			esp_mqtt_client_publish(g_mqtt_client, "soil", buffer_, 0, 0, 0);
		}
        vTaskDelay(pdMS_TO_TICKS(1000));
    }

}

/**
 * @description: Modify the format of sending password and card number to BCD
 * @param {char} v
 * @return {*}
 */
/*
static unsigned char hexToBCD(char v){return (v>='0'&&v<='9'?(v-'0'):(v>='a'&&v<='f'?(v-'a'+10):(v>='A'&&v<='F'?(v-'A'+10):0)));}
int decTobcd(int decimal)
{
	int sum = 0, i;
	for ( i = 0; decimal > 0; i++)
	{
		sum |= ((decimal % 10 ) << ( 4*i));
		decimal /= 10;
	}
	return sum;
}
*/

// Wait BLE data timer
esp_ble_gattc_cb_param_t *p_data_gb; 
esp_gatt_if_t gattc_if_gb;
uint8_t ble_timeout_flag = 1;

/**
 * @description: Connect timer, connect timeout, disconnect Bluetooth, and clear the global command, BLE connect
 * @param {void} *arg
 * @return {*}
 */
void BLE_timeout_timer(void *arg)
{
    if (ble_timeout_flag)
    {
        if(0 != g_msg_cmd){
            ble_empty_cmd_data();
        }
        ESP_LOGI(TAG, "Ble data wait timeout.");
        ble_is_connected = false;
        esp_ble_gattc_close(gattc_if_gb, gl_profile_tab[PROFILE_A_APP_ID].conn_id);
        esp_ble_gap_disconnect(p_data_gb->connect.remote_bda);
    }
    else
    {
        ESP_LOGI(TAG, "Ble data success.");
        ble_timeout_flag = 1;
    }
}

// BLE conect timer config
esp_timer_handle_t timer_once_hanle = 0;
esp_timer_create_args_t test_once_arg = {
		.callback = &BLE_timeout_timer,// callback
		.arg = NULL,// no args
		.name = "BLETIMER"// timer name
};

/**
 * @description: MQTT through the platform to get data for processing
 * @param {esp_mqtt_event_handle_t} event
 * @return {*}
 */
void xv_mqtt_event_hanler(esp_mqtt_event_handle_t event)
{   
    // Feed dog
    rtc_wdt_feed();
    esp_task_wdt_reset();
    char temp[3];
    char *data = event->data;
    // ??????#??????#??????
    if (*data == 'o')
    {
        // ssd1306_display_text_x3(&dev, 2, "AirGO", 5, false);
        if (*(data + 1) == 'n')
        {
            ac_current_info.on = true;
            ESP_LOGE(TAG, "ON");
            // ??????????????????????????? on#2
            if (*(data + 3) == '2')
            {
                ESP_LOGE(TAG, "COOL_MODE");
                ac_current_info.mode = COOL_MODE;
            }
            // ??????????????????????????? on#3
            else if (*(data + 3) == '3')
            {
                ESP_LOGE(TAG, "HEAT_MODE");
                ac_current_info.mode = HEAT_MODE;
            }
            // ??????????????????????????? on#4
            else if (*(data + 3) == '4')
            {
                ESP_LOGE(TAG, "SUPPLY_MODE");
                ac_current_info.mode = DRY_MODE;
                ac_current_info.temp = 31;
                ac_send_r05d_code(ac_current_info);
                return;
            }
            // ??????????????????????????? on#5
            else if (*(data + 3) == '5')
            {
                ESP_LOGE(TAG, "DRY_MODE");
                ac_current_info.mode = DRY_MODE;
            }
            // ??????????????????????????? on#6
            else if (*(data + 3) == '6')
            {
                ESP_LOGE(TAG, "SLEEP_MODE");
                ac_current_info.mode = SLEEP_MODE;
            }
            // ??????????????????????????? on#7
            else if (*(data + 3) == '7')
            {
                ESP_LOGE(TAG, "ECO_MODE");
                ac_current_info.mode = ECO_MODE;
            }
            // ?????????16??????????????? on#?????????#16
            memcpy(temp, data+5, 2);
            char temp_str[5];
            sprintf(temp_str, " %dC ", atoi(temp));
            // ssd1306_display_text_x3(&dev, 5, temp_str, 5, false);
            ESP_LOGE(TAG, "Temp: %d", atoi(temp));

        }
        else if(*(data +1) == 'f')
        {
            ac_current_info.on = false;
            // ssd1306_display_text_x3(&dev, 5, " OFF ", 5, false);
            ESP_LOGE(TAG, "OFF");
        }
        ac_current_info.temp = atoi(temp);
        ac_send_r05d_code(ac_current_info);
    }
}

/**
 * @description: MQTT ERROR CALLBACK
 * @param {char} *message
 * @param {int} error_code
 * @return {*}
 */
static void log_error_if_nonzero(const char *message, int error_code)
{
    if (error_code != 0) {
        ESP_LOGE(TAG, "Last error %s: 0x%x", message, error_code);
    }
}
/**
 * @description:  MQTT PROCESS CALLBACK
 * @param {void} *handler_args
 * @param {esp_event_base_t} base
 * @param {int32_t} event_id
 * @param {void} *event_data
 * @return {*}
 */
static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data)
{
    ESP_LOGD(TAG, "Event dispatched from event loop base=%s, event_id=%d", base, event_id);
    esp_mqtt_event_handle_t event = event_data;
    g_mqtt_client = event->client;
    int msg_id;
    switch ((esp_mqtt_event_id_t)event_id) {
    case MQTT_EVENT_CONNECTED:
        g_led_flag = 1;
        ESP_LOGI(TAG, "MQTT_EVENT_CONNECTED");
        vTaskDelay(2000 / portTICK_RATE_MS);

		// server topic subscribe previate
        msg_id = esp_mqtt_client_subscribe(g_mqtt_client, TOPIC, QOS1);
        // vTaskDelay(2000 / portTICK_RATE_MS);
		// server topic subscribe public
        msg_id = esp_mqtt_client_subscribe(g_mqtt_client, "public", QOS1);
        ESP_LOGI(TAG, "sent subscribe successful, msg_id=%d", msg_id);
        ESP_LOGI(TAG, "Public and Private Topic subscribe successfull.");

        // DEV connect server suuces, send data ask server devOnline
        if(json_start())
        {
            json_put_int("command", 0);
            json_split();
            json_put_int("version", g_version);
            
            json_end();
            char *buffer = json_buffer();
            esp_mqtt_client_publish(g_mqtt_client, g_topic_up, buffer, 0, QOS1, 0);
        }
        ESP_LOGI(TAG, "Sent Update Time publish successful, msg_id=%d", msg_id);

        // vTaskDelay(2000 / portTICK_RATE_MS);

        // UPDATE SUB LOCK LIST
        if(json_start())
        {
            json_put_int("command", 1);
            json_split();
            json_put_int("version", g_version);
            
            json_end();
            char *buffer = json_buffer();
            esp_mqtt_client_publish(g_mqtt_client, g_topic_up, buffer, 0, QOS1, 0);
        }
        ESP_LOGI(TAG, "Sent Update Lock list publish successful, msg_id=%d", msg_id);

        break;
    case MQTT_EVENT_DISCONNECTED:
        ESP_LOGI(TAG, "MQTT_EVENT_DISCONNECTED");
        g_led_flag = 0;
        // MQTT Disconnect, Reconnect wifi or restart system;
        break;

    case MQTT_EVENT_SUBSCRIBED:
        ESP_LOGI(TAG, "MQTT_EVENT_SUBSCRIBED, msg_id=%d", event->msg_id);
        // msg_id = esp_mqtt_client_publish(client, "/topic/qos0", "data", 0, 0, 0);
        // ESP_LOGI(TAG, "sent publish successful, msg_id=%d", msg_id);
        break;
    case MQTT_EVENT_UNSUBSCRIBED:
        ESP_LOGI(TAG, "MQTT_EVENT_UNSUBSCRIBED, msg_id=%d", event->msg_id);
        break;
    case MQTT_EVENT_PUBLISHED:
        // ESP_LOGI(TAG, "MQTT_EVENT_PUBLISHED, msg_id=%d", event->msg_id);
        break;
    case MQTT_EVENT_DATA:
        ESP_LOGI(TAG, "MQTT_EVENT_DATA");
        ESP_LOGI(TAG, "TOPIC=%.*s", event->topic_len, event->topic);
        ESP_LOGI(TAG, "DATA=%.*s", event->data_len, event->data);

        xv_mqtt_event_hanler(event);

        break;
    case MQTT_EVENT_ERROR:
        ESP_LOGI(TAG, "MQTT_EVENT_ERROR");
        if (event->error_handle->error_type == MQTT_ERROR_TYPE_TCP_TRANSPORT) {
            log_error_if_nonzero("reported from esp-tls", event->error_handle->esp_tls_last_esp_err);
            log_error_if_nonzero("reported from tls stack", event->error_handle->esp_tls_stack_err);
            log_error_if_nonzero("captured as transport's socket errno",  event->error_handle->esp_transport_sock_errno);
            ESP_LOGI(TAG, "Last errno string (%s)", strerror(event->error_handle->esp_transport_sock_errno));
            esp_restart();
        }
        break;
    default:
        ESP_LOGI(TAG, "Other event id:%d", event->event_id);
        break;
    }
}

/**
 * @description: MQTT config and connect.
 * @param {char*} gateway mac
 * @return {*}
 */
static void mqtt_app_start(char* mac)
{
    esp_mqtt_client_config_t mqtt_cfg = {
        .host = HOST_NAME,
        .port = HOST_PORT,
        .client_id = CLIENT_ID
        // .host = "cyupi.top",
        // .port = 1883,
        // .username = "ethan",
        // .password = "cgy233..",
        // .host = "device.smartxwei.com",
        // .port = 8091,
    };

    g_mqtt_client = esp_mqtt_client_init(&mqtt_cfg);
    /* The last argument may be used to pass data to the event handler, in this example mqtt_event_handler */
    esp_mqtt_client_register_event(g_mqtt_client, ESP_EVENT_ANY_ID, mqtt_event_handler, NULL);
    esp_mqtt_client_start(g_mqtt_client);
}


static void gattc_profile_event_handler(esp_gattc_cb_event_t event, esp_gatt_if_t gattc_if, esp_ble_gattc_cb_param_t *param)
{
    esp_ble_gattc_cb_param_t *p_data = (esp_ble_gattc_cb_param_t *)param;
    p_data_gb = p_data;
    if(0 != gattc_if){
        gattc_if_gb = gattc_if;
    }

    switch (event)
    {
    case ESP_GATTC_REG_EVT:
        ESP_LOGI(TAG, "REG_EVT");
        esp_err_t scan_ret = esp_ble_gap_set_scan_params(&ble_scan_params);
        if (scan_ret)
        {
            ESP_LOGE(TAG, "set scan params error, error code = %x", scan_ret);
        }
        break;
    case ESP_GATTC_CONNECT_EVT:
    {
        ESP_LOGI(TAG, "ESP_GATTC_CONNECT_EVT conn_id %d, if %d", p_data->connect.conn_id, gattc_if);
        gl_profile_tab[PROFILE_A_APP_ID].conn_id = p_data->connect.conn_id;
        memcpy(gl_profile_tab[PROFILE_A_APP_ID].remote_bda, p_data->connect.remote_bda, sizeof(esp_bd_addr_t));
        ESP_LOGI(TAG, "REMOTE BDA:");
        esp_log_buffer_hex(TAG, gl_profile_tab[PROFILE_A_APP_ID].remote_bda, sizeof(esp_bd_addr_t));
        esp_err_t mtu_ret = esp_ble_gattc_send_mtu_req(gattc_if, p_data->connect.conn_id);
        if (mtu_ret)
        {
            ESP_LOGE(TAG, "config MTU error, error code = %x", mtu_ret);
        }
        break;
    }
    case ESP_GATTC_OPEN_EVT:
    {
        // feed task watch dog
        esp_task_wdt_reset();
        if (param->open.status != ESP_GATT_OK)
        {
            ESP_LOGE(TAG, "open failed, status %d", p_data->open.status);
            if(0 != g_msg_cmd)
            {
                // Lock open fail response status 1
                ble_empty_cmd_data();
            }
        }
        else
        {
            ble_is_connected = true;
            ESP_LOGI(TAG, "open success");
        }
        break;
    }
    case ESP_GATTC_DIS_SRVC_CMPL_EVT:
        if (param->dis_srvc_cmpl.status != ESP_GATT_OK)
        {
            ESP_LOGE(TAG, "discover service failed, status %d", param->dis_srvc_cmpl.status);
            break;
        }
        ESP_LOGI(TAG, "discover service complete conn_id %d", param->dis_srvc_cmpl.conn_id);
        esp_ble_gattc_search_service(gattc_if, param->cfg_mtu.conn_id, &remote_filter_service_uuid);
        break;
    case ESP_GATTC_CFG_MTU_EVT:
        if (param->cfg_mtu.status != ESP_GATT_OK)
        {
            ESP_LOGE(TAG, "config mtu failed, error status = %x", param->cfg_mtu.status);
        }
        ESP_LOGI(TAG, "ESP_GATTC_CFG_MTU_EVT, Status %d, MTU %d, conn_id %d", param->cfg_mtu.status, param->cfg_mtu.mtu, param->cfg_mtu.conn_id);
        break;
    case ESP_GATTC_SEARCH_RES_EVT:
    {
        ESP_LOGI(TAG, "SEARCH RES: conn_id = %x is primary service %d", p_data->search_res.conn_id, p_data->search_res.is_primary);
        ESP_LOGI(TAG, "start handle %d end handle %d current handle value %d", p_data->search_res.start_handle, p_data->search_res.end_handle, p_data->search_res.srvc_id.inst_id);
        if (p_data->search_res.srvc_id.uuid.len == ESP_UUID_LEN_16 && p_data->search_res.srvc_id.uuid.uuid.uuid16 == REMOTE_SERVICE_UUID)
        {
            ESP_LOGI(TAG, "service found");
            get_server = true;
            gl_profile_tab[PROFILE_A_APP_ID].service_start_handle = p_data->search_res.start_handle;
            gl_profile_tab[PROFILE_A_APP_ID].service_end_handle = p_data->search_res.end_handle;
            ESP_LOGI(TAG, "UUID16: %x", p_data->search_res.srvc_id.uuid.uuid.uuid16);
        }
        break;
    }
    case ESP_GATTC_SEARCH_CMPL_EVT:
        if (p_data->search_cmpl.status != ESP_GATT_OK)
        {
            ESP_LOGE(TAG, "search service failed, error status = %x", p_data->search_cmpl.status);
            break;
        }
        if (p_data->search_cmpl.searched_service_source == ESP_GATT_SERVICE_FROM_REMOTE_DEVICE)
        {
            ESP_LOGI(TAG, "Get service information from remote device");
        }
        else if (p_data->search_cmpl.searched_service_source == ESP_GATT_SERVICE_FROM_NVS_FLASH)
        {
            ESP_LOGI(TAG, "Get service information from flash");
        }
        else
        {
            ESP_LOGI(TAG, "unknown service source");
        }
        ESP_LOGI(TAG, "ESP_GATTC_SEARCH_CMPL_EVT");
        if (get_server)
        {
            uint16_t count = 0;
            esp_gatt_status_t status = esp_ble_gattc_get_attr_count(gattc_if,
                                                                    p_data->search_cmpl.conn_id,
                                                                    ESP_GATT_DB_CHARACTERISTIC,
                                                                    gl_profile_tab[PROFILE_A_APP_ID].service_start_handle,
                                                                    gl_profile_tab[PROFILE_A_APP_ID].service_end_handle,
                                                                    INVALID_HANDLE,
                                                                    &count);
            if (status != ESP_GATT_OK)
            {
                ESP_LOGE(TAG, "esp_ble_gattc_get_attr_count error");
            }

            if (count > 0)
            {
                char_elem_result = (esp_gattc_char_elem_t *)malloc(sizeof(esp_gattc_char_elem_t) * count);
                if (!char_elem_result)
                {
                    ESP_LOGE(TAG, "gattc no mem");
                }
                else
                {
                    status = esp_ble_gattc_get_char_by_uuid(gattc_if,
                                                            p_data->search_cmpl.conn_id,
                                                            gl_profile_tab[PROFILE_A_APP_ID].service_start_handle,
                                                            gl_profile_tab[PROFILE_A_APP_ID].service_end_handle,
                                                            remote_filter_char_uuid,
                                                            char_elem_result,
                                                            &count);
                    if (status != ESP_GATT_OK)
                    {
                        ESP_LOGE(TAG, "esp_ble_gattc_get_char_by_uuid error");
                    }

                    /*  Every service have only one char in our 'ESP_GATTS_DEMO' demo, so we used first 'char_elem_result' */
                    if (count > 0 && (char_elem_result[0].properties & ESP_GATT_CHAR_PROP_BIT_NOTIFY))
                    {
                        gl_profile_tab[PROFILE_A_APP_ID].char_handle = char_elem_result[0].char_handle;
                        esp_ble_gattc_register_for_notify(gattc_if, gl_profile_tab[PROFILE_A_APP_ID].remote_bda, char_elem_result[0].char_handle);
                    }
                }
                /* free char_elem_result */
                free(char_elem_result);
            }
            else
            {
                ESP_LOGE(TAG, "no char found");
            }
        }
        break;
    case ESP_GATTC_REG_FOR_NOTIFY_EVT:
    {
        ESP_LOGI(TAG, "ESP_GATTC_REG_FOR_NOTIFY_EVT");
        if (p_data->reg_for_notify.status != ESP_GATT_OK)
        {
            ESP_LOGE(TAG, "REG FOR NOTIFY failed: error status = %d", p_data->reg_for_notify.status);
        }
        else
        {
            uint16_t count = 0;
            uint16_t notify_en = 1;
            esp_gatt_status_t ret_status = esp_ble_gattc_get_attr_count(gattc_if,
                                                                        gl_profile_tab[PROFILE_A_APP_ID].conn_id,
                                                                        ESP_GATT_DB_DESCRIPTOR,
                                                                        gl_profile_tab[PROFILE_A_APP_ID].service_start_handle,
                                                                        gl_profile_tab[PROFILE_A_APP_ID].service_end_handle,
                                                                        gl_profile_tab[PROFILE_A_APP_ID].char_handle,
                                                                        &count);
            if (ret_status != ESP_GATT_OK)
            {
                ESP_LOGE(TAG, "esp_ble_gattc_get_attr_count error");
            }
            if (count > 0)
            {
                descr_elem_result = malloc(sizeof(esp_gattc_descr_elem_t) * count);
                if (!descr_elem_result)
                {
                    ESP_LOGE(TAG, "malloc error, gattc no mem");
                }
                else
                {
                    ret_status = esp_ble_gattc_get_descr_by_char_handle(gattc_if,
                                                                        gl_profile_tab[PROFILE_A_APP_ID].conn_id,
                                                                        p_data->reg_for_notify.handle,
                                                                        notify_descr_uuid,
                                                                        descr_elem_result,
                                                                        &count);
                    if (ret_status != ESP_GATT_OK)
                    {
                        ESP_LOGE(TAG, "esp_ble_gattc_get_descr_by_char_handle error");
                    }
                    /* Every char has only one descriptor in our 'ESP_GATTS_DEMO' demo, so we used first 'descr_elem_result' */
                    if (count > 0 && descr_elem_result[0].uuid.len == ESP_UUID_LEN_16 && descr_elem_result[0].uuid.uuid.uuid16 == ESP_GATT_UUID_CHAR_CLIENT_CONFIG)
                    {
                        vTaskDelay(200 / portTICK_PERIOD_MS);
                        ret_status = esp_ble_gattc_write_char_descr(gattc_if,
                                                                    gl_profile_tab[PROFILE_A_APP_ID].conn_id,
                                                                    descr_elem_result[0].handle,
                                                                    sizeof(notify_en),
                                                                    (uint8_t *)&notify_en,
                                                                    ESP_GATT_WRITE_TYPE_RSP,
                                                                    ESP_GATT_AUTH_REQ_NONE);
                    }
                    if (ret_status != ESP_GATT_OK)
                    {
                        ESP_LOGE(TAG, "esp_ble_gattc_write_char_descr error");
                    }

                    /* free descr_elem_result */
                    free(descr_elem_result);
                }
            }
            else
            {
                ESP_LOGE(TAG, "decsr not found");
            }
        }
        break;
    }
    case ESP_GATTC_NOTIFY_EVT:
    {
        if (p_data->notify.is_notify)
        {
            ESP_LOGI(TAG, "ESP_GATTC_NOTIFY_EVT, receive notify value:");
        }
        else
        {
            ESP_LOGI(TAG, "ESP_GATTC_NOTIFY_EVT, receive indicate value:");
        }

        break;
    }
    case ESP_GATTC_WRITE_DESCR_EVT:
        if (p_data->write.status != ESP_GATT_OK)
        {
            ESP_LOGE(TAG, "write descr failed, error status = %x", p_data->write.status);
            break;
        }
        ESP_LOGI(TAG, "write descr success ");
        vTaskDelay(200 / portTICK_RATE_MS);
        // cgytest Send Ping
        esp_ble_gattc_write_char(gattc_if,
                                 gl_profile_tab[PROFILE_A_APP_ID].conn_id,
                                 gl_profile_tab[PROFILE_A_APP_ID].char_handle,
                                 20,
                                 g_pin,
                                 ESP_GATT_WRITE_TYPE_NO_RSP,
                                 ESP_GATT_AUTH_REQ_NONE);
        break;
    case ESP_GATTC_SRVC_CHG_EVT:
    {
        esp_bd_addr_t bda;
        memcpy(bda, p_data->srvc_chg.remote_bda, sizeof(esp_bd_addr_t));
        ESP_LOGI(TAG, "ESP_GATTC_SRVC_CHG_EVT, bd_addr:");
        esp_log_buffer_hex(TAG, bda, sizeof(esp_bd_addr_t));
        break;
    }
    case ESP_GATTC_WRITE_CHAR_EVT:
        if (p_data->write.status != ESP_GATT_OK)
        {
            ESP_LOGE(TAG, "write char failed, error status = %x", p_data->write.status);
            break;
        }
        ESP_LOGI(TAG, "write char success ");
        break;
    case ESP_GATTC_DISCONNECT_EVT:
    {
        get_server = false;
        ESP_LOGI(TAG, "ESP_GATTC_DISCONNECT_EVT, reason = %d", p_data->disconnect.reason);
        ESP_LOGI(TAG, "Gattc disconnect done.");
        ble_empty_cmd_data();
        break;
    }
    default:
        break;
    }
}

static void esp_gap_cb(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param)
{
    uint8_t *adv_name = NULL;
    uint8_t adv_name_len = 0;
    switch (event)
    {
    case ESP_GAP_BLE_SCAN_PARAM_SET_COMPLETE_EVT:
    {
        g_ble_scan_param_complete = true;
        break;
    }
    case ESP_GAP_BLE_SCAN_START_COMPLETE_EVT:
    {
        esp_task_wdt_reset();

        // scan start complete event to indicate scan start successfully or failed
        if (param->scan_start_cmpl.status != ESP_BT_STATUS_SUCCESS)
        {
            ESP_LOGE(TAG, "scan start failed, error status = %x", param->scan_start_cmpl.status);
            ESP_LOGI(TAG, "****************Scan start Fail, Gateway restart.*****************");
            esp_restart();
            break;
        }
        // ESP_LOGI(TAG, "scan start success");
        g_ble_scaning = true;

        break;
    }
    case ESP_GAP_BLE_SCAN_RESULT_EVT:
    {
        esp_ble_gap_cb_param_t *scan_result = (esp_ble_gap_cb_param_t *)param;
        switch (scan_result->scan_rst.search_evt)
        {
        case ESP_GAP_SEARCH_INQ_RES_EVT:
            // LOG SCANNED Device info.
            // esp_log_buffer_hex(TAG, scan_result->scan_rst.bda, 6);
            // ESP_LOGI(TAG, "searched Adv Data Len %d, Scan Response Len %d", scan_result->scan_rst.adv_data_len, scan_result->scan_rst.scan_rsp_len);
            adv_name = esp_ble_resolve_adv_data(scan_result->scan_rst.ble_adv,
                                                ESP_BLE_AD_TYPE_NAME_CMPL, &adv_name_len);
            // ESP_LOGI(TAG, "searched Device Name Len %d", adv_name_len);
            // esp_log_buffer_char(TAG, adv_name, adv_name_len);
            if (adv_name != NULL)
            {
                g_mi_band_rssi = scan_result->scan_rst.rssi;
                if (g_mi_band_rssi < 0)
                {
                    g_mi_band_rssi = ~g_mi_band_rssi;
                }
                // sprintf(rssi_str, " %d", g_mi_band_rssi);
                // ssd1306_display_text_x3(&dev, 5, rssi_str, 5, false);
                char band[] = "Mi Smart Band 6";
                ESP_LOGD(TAG, "Device name: %s, RSSI: %d", band, g_mi_band_rssi);
                // // UPDATE SUB LOCK LIST/
                // if(json_start())
                // {
                //     json_put_string("device_name", band);
                //     json_split();
                //     json_put_int("rssi", g_mi_band_rssi);
                    
                //     json_end();
                //     char *buffer = json_buffer();
                //     esp_mqtt_client_publish(g_mqtt_client, g_topic_up, buffer, 0, QOS1, 0);
                // }
            }

            if (adv_name != NULL
                && memcmp(adv_name, "TM", 2) == 0)
            {
                // printf("scaned drived name: %s", adv_name);

                if(strlen((char*)adv_name) > BLE_NAME_LEN){
                    adv_name += (strlen((char*)adv_name) - BLE_NAME_LEN);
                   //printf("device_name more: %s", adv_name);
                }
                for (int i = 0; i < XV_LOCK_LIST_LENGTH; i++)
                {
                    if(g_device_list[i].sn[0]
                        && ble_device_check(&g_device_list[i], adv_name))
                    {
                        ble_device_set_dev(&g_device_list[i], scan_result);
                        ESP_LOGI(TAG, "Device name: %s, RSSI: %d", adv_name, scan_result->scan_rst.rssi);
                        // ESP_LOGI(TAG, "********************NowTimeStamp: %lld********************", nowTimeStamp());
                        g_device_list[i].checked = true;
                        g_device_list[i].rssi = scan_result->scan_rst.rssi;
                        // lock have message to report
                        if ((&scan_result->scan_rst.ble_adv[0])[0] == 0x07 
                            && (&scan_result->scan_rst.ble_adv[0])[1] == 0xff)
                        {
                            g_device_list[i].has_data = true;
                    
                        }
                        else{
                            g_device_list[i].has_data = false;
                        }
                        g_device_list[i].online = true;
                    }
                    else{
                        ;//ESP_LOGI(TAG, "device_name not: %s", adv_name);
                    }
                }
            }
            break;

        case ESP_GAP_SEARCH_INQ_CMPL_EVT:
        {
            
            char band[] = "Mi Smart Band 6";
            sprintf(rssi_str, "%ddBm", g_mi_band_rssi);
            ssd1306_display_text_x3(&dev, 5, rssi_str, 5, false);
            ESP_LOGI(TAG, "Device name: %s, RSSI: %d", band, g_mi_band_rssi);
            // UPDATE SUB LOCK LIST/
            if(json_start())
            {
                json_put_string("device_name", band);
                json_split();
                json_put_int("rssi", g_mi_band_rssi);
                
                json_end();
                char *buffer = json_buffer();
                esp_mqtt_client_publish(g_mqtt_client, g_topic_up, buffer, 0, QOS1, 0);
            }
            g_mi_band_rssi = 0;
            // ESP_LOGI(TAG, "****************End of one scan cycle.*****************");
            /*
            g_mi_band_rssi = 0;
            for (int i = 0; i < XV_LOCK_LIST_LENGTH; i++)
            {
                if(g_device_list[i].checked)
                {
                    g_device_list[i].no_checked = 0;
                    g_device_list[i].online = true;
                }
                else
                {
                    ++ g_device_list[i].no_checked;
                    if(g_device_list[i].no_checked >= MAX_SCAN_TIMES){
                        g_device_list[i].online = false;
                        g_device_list[i].rssi = 0;
                    }
                }
            }
            */
            // FEED DOG
            esp_task_wdt_reset();
            rtc_wdt_feed();
            g_ble_scan_count = (g_scan_max + 1);
            g_ble_scaning = false;
            break;
        }
        default:
            break;
        }
        break;
    }

    case ESP_GAP_BLE_SCAN_STOP_COMPLETE_EVT:
        if (param->scan_stop_cmpl.status != ESP_BT_STATUS_SUCCESS)
        {
            ESP_LOGE(TAG, "scan stop failed, error status = %x", param->scan_stop_cmpl.status);
            break;
        }
        ESP_LOGI(TAG, "stop scan successfully");
        g_ble_scaning = false;
        break;

    case ESP_GAP_BLE_ADV_STOP_COMPLETE_EVT:
        if (param->adv_stop_cmpl.status != ESP_BT_STATUS_SUCCESS)
        {
            ESP_LOGE(TAG, "adv stop failed, error status = %x", param->adv_stop_cmpl.status);
            break;
        }
        ESP_LOGI(TAG, "stop adv successfully");
        break;
    case ESP_GAP_BLE_UPDATE_CONN_PARAMS_EVT:
        ESP_LOGI(TAG, "update connection params status = %d, min_int = %d, max_int = %d,conn_int = %d,latency = %d, timeout = %d",
                 param->update_conn_params.status,
                 param->update_conn_params.min_int,
                 param->update_conn_params.max_int,
                 param->update_conn_params.conn_int,
                 param->update_conn_params.latency,
                 param->update_conn_params.timeout);
        break;
    default:
        break;
    }
}

static void esp_gattc_cb(esp_gattc_cb_event_t event, esp_gatt_if_t gattc_if, esp_ble_gattc_cb_param_t *param)
{
    /* If event is register event, store the gattc_if for each profile */
    if (event == ESP_GATTC_REG_EVT)
    {
        if (param->reg.status == ESP_GATT_OK)
        {
            gl_profile_tab[param->reg.app_id].gattc_if = gattc_if;
        }
        else
        {
            ESP_LOGI(TAG, "reg app failed, app_id %04x, status %d",
                     param->reg.app_id,
                     param->reg.status);
            return;
        }
    }

    /* If the gattc_if equal to profile A, call profile A cb handler,
     * so here call each profile's callback */

    do
    {
        int idx;
        for (idx = 0; idx < PROFILE_NUM; idx++)
        {
            if (gattc_if == ESP_GATT_IF_NONE || /* ESP_GATT_IF_NONE, not specify a certain gatt_if, need to call every profile cb function */
                gattc_if == gl_profile_tab[idx].gattc_if)
            {
                if (gl_profile_tab[idx].gattc_cb)
                {
                    gl_profile_tab[idx].gattc_cb(event, gattc_if, param);
                }
            }
        }

    } while (0);
}
/*
*/
void check_lock_data()
{
    int i;
    for (i = 0; i < XV_LOCK_LIST_LENGTH; i++)
    {
        if (g_device_list[i].has_data)
        {
            ESP_LOGI(TAG, "********************check_data********************");
            ESP_LOGI(TAG, "Has data lock sn: %s", g_device_list[i].sn);
            memset(g_buffer_set, 0, MSG_MAX_BUFFER);
            msg_open *open = (msg_open * )g_buffer_set;
            open->command = XV_GW_OPEN_LOG;
            memcpy(open->sn, g_device_list[i].sn, BLE_NAME_LEN);
            msg_set((msg_base * )open);
        }
    }
}
/*
*/
void ble_scan()
{
    ble_is_connected = false;
    esp_ble_gattc_close(gattc_if_gb, gl_profile_tab[PROFILE_A_APP_ID].conn_id);
    esp_ble_gap_disconnect(p_data_gb->connect.remote_bda);
    // ESP_LOGI(TAG, "begin esp_ble_gap_start_scanning");
    if(!g_ble_scaning)
    {
        // ESP_LOGI(TAG, "esp_ble_gap_start_scanning...");
        for (int i = 0; i < XV_LOCK_LIST_LENGTH; i++){
            g_device_list[i].checked = 0;
            g_device_list[i].rssi = 0;
        }
        esp_ble_gap_start_scanning(duration);
    }
    g_ble_scaning = true;
}
/*
*/
void send_heart_beast()
{
    // char band[] = "Mi Smart Band 6";
    // ESP_LOGI(TAG, "Device name: %s, RSSI: %d", band, g_mi_band_rssi);
    // // UPDATE SUB LOCK LIST/
    // if(json_start())
    // {
    //     json_put_string("device_name", band);
    //     json_split();
    //     json_put_int("rssi", g_mi_band_rssi);
        
    //     json_end();
    //     char *buffer = json_buffer();
    //     esp_mqtt_client_publish(g_mqtt_client, g_topic_up, buffer, 0, QOS1, 0);
    // }
    // test ping baidu
    // ping_baidu();

    // feed dog
    esp_task_wdt_reset();
    rtc_wdt_feed();
}
/**
 * @description: Lock list maintain manager, process command in queue, report hearbeat;
 * @param {void} *param
 * @return {*}
 */
void task_lock_list_maintain(void *param)
{
    int n = 0;
    int j = 0;
    int check_times = 0;

    // clear buffer
    for(int i = 0; i < MSG_MAX_COUNT; i ++){
        memset(g_buffer_list[i], 0, MSG_MAX_BUFFER);
    }

    // test mi band 6
    uint8_t tmp_bda[6] = {0xc2, 0x92, 0x7f, 0xb7, 0xe1, 0xf8};
    esp_ble_gap_update_whitelist(true, tmp_bda, BLE_WL_ADDR_TYPE_PUBLIC);

    ble_empty_cmd_data();
    
    while(1)
    {
        // heartbeat
        if (j >= 10)
        {
            // send_heart_beast();
            j = 0;
        }
        j += 1;
        n += 1;
        if(g_ble_scan_param_complete)
        {
            // level 1
            if(0 != g_msg_cmd)
            {
                ESP_LOGI(TAG, "Connect to the lock %d times.", check_times);
                ++ check_times;
                if(check_times >= 10)
                {
                    // Connect timeout
                    check_times = 0;

                    // Lock Offline report
                    ble_empty_cmd_data();
                    ESP_LOGI(TAG, "Ble data wait timeout.");

                    ble_is_connected = false;
                    esp_ble_gattc_close(gattc_if_gb, gl_profile_tab[PROFILE_A_APP_ID].conn_id);
                    esp_ble_gap_disconnect(p_data_gb->connect.remote_bda);
                }
            }
            else
            {
                g_ble_scan_count += 1;
                check_times = 0;
                // ESP_LOGI(TAG, "ssssss: %d-%d-%d", g_ble_scan_count, g_ble_scaning? 1:0, g_scan_max);
                if(g_scan_max >= 15){
                    g_scan_max = 15;
                }

                if(g_ble_scan_count >= 15){
                    g_ble_scan_count = 1;
                }
                // leve 2
                if(g_ble_scan_count >= 1 
                    && g_ble_scan_count <= g_scan_max)
                {
                    if(g_ble_scan_count == 1 || !g_ble_scaning){
                        ble_scan();
                    }
                    // ESP_LOGI(TAG, "scan time: %d-%d", g_ble_scan_count, g_ble_scaning? 1:0);
                }
                // leve 3
                else
                {
                    if(g_ble_scaning)
                    {
                        ESP_LOGI(TAG, "esp_ble_gap_stop_scanning: %d", n);
                        esp_ble_gap_stop_scanning();
                        g_ble_scaning = false;
                    }
                    g_msg_cmd = msg_get();

                    if(0 != g_msg_cmd)
                    {
                        g_scan_max = 5;
                        memcpy(g_buffer_cmd, g_msg_cmd, MSG_MAX_BUFFER);
                        ble_empty_cmd_data();
                        g_msg_cmd = (msg_base *)g_buffer_cmd;
                        // do something()
                        g_current_decvice = -1;
                        //ESP_LOGI(TAG, "Searched device %s", lock_sn);
                        //bt_ble_device_connect = true;
                        
                        ble_device *pdevice = 0;
                        for(int i = 0; i < XV_LOCK_LIST_LENGTH; i++)
                        {
                            if(ble_device_check(&g_device_list[i], (uint8_t *)g_msg_cmd->sn))
                            {
                                pdevice = &g_device_list[i];
                                g_current_decvice = i;
                                break;
                            }
                        }
                        if(pdevice)
                        {
                            ESP_LOGI(TAG, "Lock SN: %s", pdevice->sn);
                            // static key init
                            for (int j = 12; j < 16; j++){
                                g_static_key[j] = 0;
                            }
                            ESP_LOGI(TAG, "Lock connectting...");
                            if(pdevice->set_dev){
                                esp_ble_gattc_open(gattc_if_gb, pdevice->dev_cb.scan_rst.bda, pdevice->dev_cb.scan_rst.ble_addr_type, true);
                            }
                            else
                            {
                                ESP_LOGI(TAG, "Lock no scaned...");
                                // Lock data not set status 1
                                ble_empty_cmd_data();
                            }
                        }
                        else
                        {
                            ESP_LOGI(TAG, "Lock no scaned...");
                            // Get lock sn but not in list, report
                            ble_empty_cmd_data();
                        }
                    }
                    else{
                        g_scan_max = 15;
                    }
                }
            }
        }
        vTaskDelay(2000 / portTICK_RATE_MS);
    }
}
/**
 * @description: Initialize BT Contraoller
 * @param {*}
 * @return {*}
 */
void BLE_init()
{
    ESP_ERROR_CHECK(esp_bt_controller_mem_release(ESP_BT_MODE_CLASSIC_BT));

    esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
    esp_err_t ret = esp_bt_controller_init(&bt_cfg);
    if (ret)
    {
        ESP_LOGE(TAG, "%s initialize controller failed: %s", __func__, esp_err_to_name(ret));
        return;
    }
    ret = esp_bt_controller_enable(ESP_BT_MODE_BLE);
    if (ret)
    {
        ESP_LOGE(TAG, "%s enable controller failed: %s", __func__, esp_err_to_name(ret));
        return;
    }

    ret = esp_bluedroid_init();
    if (ret)
    {
        ESP_LOGE(TAG, "%s init bluetooth failed: %s", __func__, esp_err_to_name(ret));
        return;
    }

    ret = esp_bluedroid_enable();
    if (ret)
    {
        ESP_LOGE(TAG, "%s enable bluetooth failed: %s", __func__, esp_err_to_name(ret));
        return;
    }
    // BT TX POWER SETTIN
    ret = esp_ble_tx_power_set(ESP_BLE_PWR_TYPE_DEFAULT, ESP_PWR_LVL_P9);
    if (ret) {
        ESP_LOGE(TAG, "%s Unable to set BLE Tx power level", __func__);
        return;
    }
    ESP_LOGI(TAG, "%s Successfully set BLE Tx power level", __func__);

    // register the  callback function to the gap module
    ret = esp_ble_gap_register_callback(esp_gap_cb);
    if (ret)
    {
        ESP_LOGE(TAG, "%s gap register failed, error code = %x", __func__, ret);
        return;
    }

    // register the callback function to the gattc module
    ret = esp_ble_gattc_register_callback(esp_gattc_cb);
    if (ret)
    {
        ESP_LOGE(TAG, "%s gattc register failed, error code = %x", __func__, ret);
        return;
    }

    ret = esp_ble_gattc_app_register(PROFILE_A_APP_ID);
    if (ret)
    {
        ESP_LOGE(TAG, "%s gattc app register failed, error code = %x", __func__, ret);
    }
    esp_err_t local_mtu_ret = esp_ble_gatt_set_local_mtu(128);
    if (local_mtu_ret)
    {
        ESP_LOGE(TAG, "set local  MTU failed, error code = %x", local_mtu_ret);
    }

    esp_err_t err = esp_timer_create(&test_once_arg, &timer_once_hanle);

    ESP_LOGI(TAG, "Bluetimeout timer create %s", err == ESP_OK ? "ok." : "failed.");
}

void rtc_wdt_init()
{
    // RTC WATCH DOG INIT
    // ESP_LOGI(TAG, "Initialize RTCWDT");
    rtc_wdt_protect_off();
    rtc_wdt_enable();
    rtc_wdt_feed();
    rtc_wdt_set_length_of_reset_signal(RTC_WDT_SYS_RESET_SIG, RTC_WDT_LENGTH_3_2us);
    rtc_wdt_set_stage(RTC_WDT_STAGE0, RTC_WDT_STAGE_ACTION_RESET_RTC);
    rtc_wdt_set_time(RTC_WDT_STAGE0, 90000);
    rtc_wdt_disable();

}

void network_config_init()
{
    ESP_LOGI(TAG, "ESP_ETHERNET_MODE_STA ");
    ethernet_init_sta();

    uint8_t connect_tick = 10;
    while(connect_tick--)
    {
        if (ethernet_connect_ok)
        {
            break;
        }
        if (1 == connect_tick)
        {
            ESP_LOGI(TAG, "ESP_WIFI_MODE_STA ");
            wifi_init_sta();
        }

        vTaskDelay(200 / portTICK_PERIOD_MS);   /* ??????500ms*/
    }

}

void mqtt_begin()
{
    // APP INIT
    uint8_t efuse_mac[6] = {0};
    char *mac = (char*)g_buf_send;
    memset(mac, 0, 32);
    // esp_read_mac(efuse_mac,ESP_MAC_ETH);
    esp_efuse_mac_get_default(efuse_mac);
    sprintf(mac, "%01x%02x%02x%02x%02x%02x", 
        efuse_mac[0],
        efuse_mac[1],
        efuse_mac[2],
        efuse_mac[3],
        efuse_mac[4],
        efuse_mac[5]);
    ESP_LOGI(TAG, "[APP] Startup..");
    ESP_LOGI(TAG, "[APP] Free memory: %d bytes", esp_get_free_heap_size());
    ESP_LOGI(TAG, "[APP] IDF version: %s", esp_get_idf_version());
    ESP_LOGI(TAG, "********************Mac: %s ********************", mac);

    esp_log_level_set("*", ESP_LOG_INFO);
    esp_log_level_set("MQTT_CLIENT", ESP_LOG_VERBOSE);
    esp_log_level_set("MQTT_EXAMPLE", ESP_LOG_VERBOSE);
    esp_log_level_set("TRANSPORT_BASE", ESP_LOG_VERBOSE);
    esp_log_level_set("esp-tls", ESP_LOG_VERBOSE);
    esp_log_level_set("TRANSPORT", ESP_LOG_VERBOSE);
    esp_log_level_set("OUTBOX", ESP_LOG_VERBOSE);

    // MQTT START
    mqtt_app_start(mac);
    
}

void nvs_init()
{
    // Initialize NVS.
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES 
        || ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

}

/**
 * @description: Procedure entrance
 * @param {*}
 * @return {*}
 */
void app_main(void)
{
    // nvs init
    nvs_init();
    //rmt init
    air_conditioner_init();
    // check firmware_version
    check_firmware_version();
    //ssd1306 init
    xTaskCreate(display_init, "task_display", 1024 * 10, NULL, 5, NULL);
    // button init
    xTaskCreate(key_trigger, "key_trigger", 1024 * 2, NULL, 10, NULL);
    // led init
    led_init();
    xTaskCreate(led_flicker, "task_led", 1024 * 10, NULL, 5, NULL);
    // SmartConfig Initialize
    wifi_smartconfig_check();
    // Network Config Initialize
    // network_config_init();
    wifi_init_sta();
    //sntp
    sntp_start();
    // RTC watch dog initialization
    rtc_wdt_init();
    // show mac and start mqtt
    mqtt_begin();
    // ble init
    BLE_init();
    // Hearbeat lock data report check 
    xTaskCreate(task_lock_list_maintain, "task_lock_list_maintain", 1024 * 10, NULL, 5, NULL);
    // Soid humidity reader
    // xTaskCreate(soid_humidity, "task_soid_read", 1024 * 10, NULL, 5, NULL);
}