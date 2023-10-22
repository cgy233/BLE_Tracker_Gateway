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

#include "esp_bt.h"
#include "esp_bt_main.h"
#include "esp_gatt_common_api.h"
#include "esp_log.h"
#include "esp_adc_cal.h"
#include "esp_task_wdt.h"
#include "soc/rtc_wdt.h"

#include "mqtt_client.h"

#include "json.h"
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
#include "ble_manager.h"
#include "mqtt.h"

extern struct gattc_profile_inst gl_profile_tab[PROFILE_NUM];

static esp_adc_cal_characteristics_t *adc_chars;
static const adc_channel_t channel = ADC_CHANNEL_6; // GPIO34 if ADC1, GPIO14 if ADC2
static const adc_bits_width_t width = ADC_WIDTH_BIT_12;
static const adc_atten_t atten = ADC_ATTEN_DB_6;
static const adc_unit_t unit = ADC_UNIT_1;

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
char g_buffer_cmd[MSG_MAX_BUFFER] = {0};

esp_mqtt_client_handle_t g_mqtt_client; // MQTT client handle

uint8_t g_buf_send[32] = {0};																							  // BLE send data
byte g_static_key[16] = {0xca, 0x5b, 0xd3, 0x8a, 0xe8, 0x59, 0x73, 0xfd, 0x77, 0x59, 0xec, 0x02, 0x00, 0x00, 0x00, 0x00}; // BLE Secret Key
byte g_dynamic_key[4] = {0};																							  // BLE dynamic key

msg_base *g_msg_cmd = 0;

uint8_t eco_flag = 0;
uint8_t sleep_flag = 0;


extern esp_gatt_if_t gattc_if_gb;
extern esp_ble_gattc_cb_param_t *p_data_gb;

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