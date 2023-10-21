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
#include "lock.h"

extern struct gattc_profile_inst gl_profile_tab[PROFILE_NUM];

static esp_adc_cal_characteristics_t *adc_chars;
static const adc_channel_t channel = ADC_CHANNEL_6; // GPIO34 if ADC1, GPIO14 if ADC2
static const adc_bits_width_t width = ADC_WIDTH_BIT_12;
static const adc_atten_t atten = ADC_ATTEN_DB_6;
static const adc_unit_t unit = ADC_UNIT_1;
// MQTT Parameter
#define HOST_NAME "cyupi.top"
#define HOST_PORT 1883
#define CLIENT_ID "ethanhomeesp32kit"
#define AIR_CMD 14 // Lock count
#define TOPIC "esp32/ethanhome/miband6"

SSD1306_t dev;

int g_mi_band_rssi = 0;

uint8_t g_version; // Firmware version

uint8_t g_led_flag = 0;			 // led status flag
uint8_t ethernet_connect_ok = 0; // led status flag

bool g_ble_scan_param_complete = false;
bool g_ble_scaning = false;
uint8_t g_ble_scan_count = 0;
short g_scan_max = 10;
bool ble_is_connected = false;

char g_buffer_list[MSG_MAX_COUNT][MSG_MAX_BUFFER];
char g_buffer_set[MSG_MAX_BUFFER] = {0};
char g_buffer_cmd[MSG_MAX_BUFFER] = {0};
int g_msg_last = 0;

esp_mqtt_client_handle_t g_mqtt_client; // MQTT client handle

char g_topic_up[32] = "esp32/ethanhome/miband6";

uint8_t g_buf_send[32] = {0};																							  // BLE send data
byte g_static_key[16] = {0xca, 0x5b, 0xd3, 0x8a, 0xe8, 0x59, 0x73, 0xfd, 0x77, 0x59, 0xec, 0x02, 0x00, 0x00, 0x00, 0x00}; // BLE Secret Key
byte g_dynamic_key[4] = {0};																							  // BLE dynamic key
static uint32_t duration = 15;																							  // Scanning period

msg_base *g_msg_cmd = 0;

uint8_t eco_flag = 0;
uint8_t sleep_flag = 0;

struct ble_devices_state_t my_ble_devices_state;

ble_device g_device_list[XV_LOCK_LIST_LENGTH];
static int g_current_decvice = -1;

AC_INFO ac_current_info = {
	.on = false,
	.mode = AUTO_MODE,
	.temp = 16,
	.fan_speed = AUTO_FAN_SPEED};

/**
 * @description: Put the instructions you receive into the queue
 * @param {msg_base} *msg
 * @return {*}
 */

static void print_char_val_type(esp_adc_cal_value_t val_type)
{
	if (val_type == ESP_ADC_CAL_VAL_EFUSE_TP)
	{
		printf("Characterized using Two Point Value\n");
	}
	else if (val_type == ESP_ADC_CAL_VAL_EFUSE_VREF)
	{
		printf("Characterized using eFuse Vref\n");
	}
	else
	{
		printf("Characterized using Default Vref\n");
	}
}

void soid_humidity()
{
	// Configure ADC
	if (unit == ADC_UNIT_1)
	{
		adc1_config_width(width);
		adc1_config_channel_atten(channel, atten);
	}
	else
	{
		adc2_config_channel_atten((adc2_channel_t)channel, atten);
	}

	// Characterize ADC
	adc_chars = calloc(1, sizeof(esp_adc_cal_characteristics_t));
	esp_adc_cal_value_t val_type = esp_adc_cal_characterize(unit, atten, width, DEFAULT_VREF, adc_chars);
	print_char_val_type(val_type);

	// Continuously sample ADC1
	while (1)
	{
		uint32_t adc_reading = 0;
		// Multisampling
		for (int i = 0; i < NO_OF_SAMPLES; i++)
		{
			if (unit == ADC_UNIT_1)
			{
				adc_reading += adc1_get_raw((adc1_channel_t)channel);
			}
			else
			{
				int raw;
				adc2_get_raw((adc2_channel_t)channel, width, &raw);
				adc_reading += raw;
			}
		}
		adc_reading /= NO_OF_SAMPLES;
		// Convert adc_reading to voltage in mV
		uint32_t voltage = esp_adc_cal_raw_to_voltage(adc_reading, adc_chars);
		printf("Raw: %d\tVoltage: %dmV\n", adc_reading, voltage);
		if (json_start())
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
uint8_t ble_timeout_flag = 1;
extern esp_gatt_if_t gattc_if_gb;

/**
 * @description: Connect timer, connect timeout, disconnect Bluetooth, and clear the global command, BLE connect
 * @param {void} *arg
 * @return {*}
 */
void BLE_timeout_timer(void *arg)
{
	if (ble_timeout_flag)
	{
		if (0 != g_msg_cmd)
		{
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
	.callback = &BLE_timeout_timer, // callback
	.arg = NULL,					// no args
	.name = "BLETIMER"				// timer name
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
	// 开关#模式#温度
	if (*data == 'o')
	{
		// ssd1306_display_text_x3(&dev, 2, "AirGO", 5, false);
		if (*(data + 1) == 'n')
		{
			ac_current_info.on = true;
			ESP_LOGE(TAG, "ON");
			// 制冷模式时，消息为 on#2
			if (*(data + 3) == '2')
			{
				ESP_LOGE(TAG, "COOL_MODE");
				ac_current_info.mode = COOL_MODE;
			}
			// 制热模式时，消息为 on#3
			else if (*(data + 3) == '3')
			{
				ESP_LOGE(TAG, "HEAT_MODE");
				ac_current_info.mode = HEAT_MODE;
			}
			// 送风模式时，消息为 on#4
			else if (*(data + 3) == '4')
			{
				ESP_LOGE(TAG, "SUPPLY_MODE");
				ac_current_info.mode = DRY_MODE;
				ac_current_info.temp = 31;
				ac_send_r05d_code(ac_current_info);
				return;
			}
			// 除湿模式时，消息为 on#5
			else if (*(data + 3) == '5')
			{
				ESP_LOGE(TAG, "DRY_MODE");
				ac_current_info.mode = DRY_MODE;
			}
			// 睡眠模式时，消息为 on#6
			else if (*(data + 3) == '6')
			{
				ESP_LOGE(TAG, "SLEEP_MODE");
				ac_current_info.mode = SLEEP_MODE;
			}
			// 节能模式时，消息为 on#7
			else if (*(data + 3) == '7')
			{
				ESP_LOGE(TAG, "ECO_MODE");
				ac_current_info.mode = ECO_MODE;
			}
			// 温度为16度，消息为 on#模式位#16
			memcpy(temp, data + 5, 2);
			char temp_str[5];
			sprintf(temp_str, " %dC ", atoi(temp));
			// ssd1306_display_text_x3(&dev, 5, temp_str, 5, false);
			ESP_LOGE(TAG, "Temp: %d", atoi(temp));
		}
		else if (*(data + 1) == 'f')
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
	if (error_code != 0)
	{
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
	switch ((esp_mqtt_event_id_t)event_id)
	{
	case MQTT_EVENT_CONNECTED:
		g_led_flag = 1;
		led_on();
		ESP_LOGI(TAG, "MQTT_EVENT_CONNECTED");
		vTaskDelay(2000 / portTICK_RATE_MS);

		// server topic subscribe previate
		// msg_id = esp_mqtt_client_subscribe(g_mqtt_client, TOPIC, QOS1);
		// vTaskDelay(2000 / portTICK_RATE_MS);
		// server topic subscribe public
		msg_id = esp_mqtt_client_subscribe(g_mqtt_client, "public", QOS1);
		ESP_LOGI(TAG, "sent subscribe successful, msg_id=%d", msg_id);
		ESP_LOGI(TAG, "Public and Private Topic subscribe successfull.");

		// DEV connect server suuces, send data ask server devOnline
		if (json_start())
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
		if (json_start())
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
		led_off();
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
		if (event->error_handle->error_type == MQTT_ERROR_TYPE_TCP_TRANSPORT)
		{
			log_error_if_nonzero("reported from esp-tls", event->error_handle->esp_tls_last_esp_err);
			log_error_if_nonzero("reported from tls stack", event->error_handle->esp_tls_stack_err);
			log_error_if_nonzero("captured as transport's socket errno", event->error_handle->esp_transport_sock_errno);
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
static void mqtt_app_start(char *mac)
{
	esp_mqtt_client_config_t mqtt_cfg = {
		.host = HOST_NAME,
		.port = HOST_PORT,
		.client_id = CLIENT_ID,
		// .host = "cyupi.top",
		// .port = 1883,
		.username = "hass",
		.password = "cgy233..",
		// .host = "device.smartxwei.com",
		// .port = 8091,
	};

	g_mqtt_client = esp_mqtt_client_init(&mqtt_cfg);
	/* The last argument may be used to pass data to the event handler, in this example mqtt_event_handler */
	esp_mqtt_client_register_event(g_mqtt_client, ESP_EVENT_ANY_ID, mqtt_event_handler, NULL);
	esp_mqtt_client_start(g_mqtt_client);
}

void mqtt_send_device_info(char *name, uint8_t confidence, int8_t rssi)
{

	// UPDATE SUB LOCK LIST/
	if (json_start())
	{
		json_put_string("name", name);
		json_split();
		json_put_int("confidence", confidence);

		json_end();
		char *buffer = json_buffer();
		esp_mqtt_client_publish(g_mqtt_client, g_topic_up, buffer, 0, QOS1, 0);
	}
}

/*
 */
/*
 */
void ble_scan()
{
	ble_is_connected = false;
	esp_ble_gattc_close(gattc_if_gb, gl_profile_tab[PROFILE_A_APP_ID].conn_id);
	esp_ble_gap_disconnect(p_data_gb->connect.remote_bda);
	// ESP_LOGI(TAG, "begin esp_ble_gap_start_scanning");
	if (!g_ble_scaning)
	{
		// ESP_LOGI(TAG, "esp_ble_gap_start_scanning...");
		for (int i = 0; i < XV_LOCK_LIST_LENGTH; i++)
		{
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
	for (int i = 0; i < MSG_MAX_COUNT; i++)
	{
		memset(g_buffer_list[i], 0, MSG_MAX_BUFFER);
	}

	// TEST MI BAND 6
	esp_ble_gap_update_whitelist(true, (uint8_t *)my_ble_devices_state.devices[0].mac_address, BLE_WL_ADDR_TYPE_PUBLIC);
	esp_ble_gap_update_whitelist(true, (uint8_t *)my_ble_devices_state.devices[1].mac_address, BLE_WL_ADDR_TYPE_PUBLIC);

	ble_empty_cmd_data();

	while (1)
	{
		// heartbeat
		if (j >= 10)
		{
			// send_heart_beast();
			j = 0;
		}
		j += 1;
		n += 1;
		if (g_ble_scan_param_complete)
		{
			// level 1
			if (0 != g_msg_cmd)
			{
				ESP_LOGI(TAG, "Connect to the lock %d times.", check_times);
				++check_times;
				if (check_times >= 10)
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
				if (g_scan_max >= 15)
				{
					g_scan_max = 15;
				}

				if (g_ble_scan_count >= 15)
				{
					g_ble_scan_count = 1;
				}
				// leve 2
				if (g_ble_scan_count >= 1 && g_ble_scan_count <= g_scan_max)
				{
					if (g_ble_scan_count == 1 || !g_ble_scaning)
					{
						ble_scan();
					}
					// ESP_LOGI(TAG, "scan time: %d-%d", g_ble_scan_count, g_ble_scaning? 1:0);
				}
				// leve 3
				else
				{
					if (g_ble_scaning)
					{
						ESP_LOGI(TAG, "esp_ble_gap_stop_scanning: %d", n);
						esp_ble_gap_stop_scanning();
						g_ble_scaning = false;
					}
					g_msg_cmd = msg_get();

					if (0 != g_msg_cmd)
					{
						g_scan_max = 5;
						memcpy(g_buffer_cmd, g_msg_cmd, MSG_MAX_BUFFER);
						ble_empty_cmd_data();
						g_msg_cmd = (msg_base *)g_buffer_cmd;
						// do something()
						g_current_decvice = -1;
						// ESP_LOGI(TAG, "Searched device %s", lock_sn);
						// bt_ble_device_connect = true;

						ble_device *pdevice = 0;
						for (int i = 0; i < XV_LOCK_LIST_LENGTH; i++)
						{
							if (ble_device_check(&g_device_list[i], (uint8_t *)g_msg_cmd->sn))
							{
								pdevice = &g_device_list[i];
								g_current_decvice = i;
								break;
							}
						}
						if (pdevice)
						{
							ESP_LOGI(TAG, "Lock SN: %s", pdevice->sn);
							// static key init
							for (int j = 12; j < 16; j++)
							{
								g_static_key[j] = 0;
							}
							ESP_LOGI(TAG, "Lock connectting...");
							if (pdevice->set_dev)
							{
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
					else
					{
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
	if (ret)
	{
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
	while (connect_tick--)
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

		vTaskDelay(200 / portTICK_PERIOD_MS); /* 延时500ms*/
	}
}

void mqtt_begin()
{
	// APP INIT
	uint8_t efuse_mac[6] = {0};
	char *mac = (char *)g_buf_send;
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
	if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
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
	// rmt init
	//  air_conditioner_init();
	//  check firmware_version
	check_firmware_version();
	// ssd1306 init
	//  xTaskCreate(display_init, "task_display", 1024 * 10, NULL, 5, NULL);
	//  button init
	//  xTaskCreate(key_trigger, "key_trigger", 1024 * 2, NULL, 10, NULL);
	//  led init
	led_init();
	// xTaskCreate(led_flicker, "task_led", 1024 * 10, NULL, 5, NULL);
	// SmartConfig Initialize
	wifi_smartconfig_check();
	// Network Config Initialize
	// network_config_init();
	wifi_init_sta();
	// sntp
	sntp_start();
	// RTC watch dog initialization
	rtc_wdt_init();
	// show mac and start mqtt
	mqtt_begin();
	// ble init
	BLE_init();
	// ble devices init
	ble_devices_init();
	// Hearbeat lock data report check
	xTaskCreate(task_lock_list_maintain, "task_lock_list_maintain", 1024 * 10, NULL, 5, NULL);
	// Soid humidity reader
	// xTaskCreate(soid_humidity, "task_soid_read", 1024 * 10, NULL, 5, NULL);
}