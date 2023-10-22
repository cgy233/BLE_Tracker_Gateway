#include "esp_bt.h"
#include "esp_bt_main.h"
#include "esp_gatt_common_api.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_event.h"
#include "esp_task_wdt.h"

#include "freertos/FreeRTOS.h"
#include "soc/rtc_wdt.h"

#include "mqtt.h"
#include "ble_manager.h"
#include "led.h"
#include "json.h"


extern ble_device g_device_list[XV_LOCK_LIST_LENGTH];
char g_buffer_set[MSG_MAX_BUFFER] = {0};
extern msg_base *g_msg_cmd;

extern struct ble_devices_state_t my_ble_devices_state;

esp_ble_gattc_cb_param_t *p_data_gb;
extern bool g_ble_scan_param_complete;
extern bool g_ble_scaning;
extern uint8_t g_ble_scan_count;
extern short g_scan_max;
extern bool ble_is_connected;
extern int g_mi_band_rssi;
extern esp_mqtt_client_handle_t g_mqtt_client; // MQTT client handle
extern char g_topic_up[32];

esp_gatt_if_t gattc_if_gb;
static bool get_server = false;
static esp_gattc_char_elem_t *char_elem_result = NULL;
static esp_gattc_descr_elem_t *descr_elem_result = NULL;

uint8_t g_pin[20] = {0x7f, 0x12, 0x0c, 0x8f, 0x92, 0x40, 0xb8, 0xc4, 0x53, 0x07, 0x3b, 0x42, 0x31, 0x51, 0xa8, 0x45, 0xdc, 0x7a, 0xfb, 0xbd}; // BLE Ping data

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

/* One gatt-based profile one app_id and one gattc_if, this array will store the gattc_if returned by ESP_GATTS_REG_EVT */
struct gattc_profile_inst gl_profile_tab[PROFILE_NUM] = {
	[PROFILE_A_APP_ID] = {
		.gattc_cb = gattc_profile_event_handler,
		.gattc_if = ESP_GATT_IF_NONE, /* Not get the gatt_if, so initial is ESP_GATT_IF_NONE */
	},
};

static uint32_t duration = 15;																							  // Scanning period

// 初始化蓝牙设备信息结构体
void init_ble_devices_state(struct ble_devices_state_t *state)
{
	// 初始化设备数量为0
	state->num_devices = 0;
}

/**
 * @description: Clear global control command
 * @param {*}
 * @return {*}
 */
void ble_empty_cmd_data()
{
	if (0 != g_msg_cmd)
	{
		memset((char *)g_msg_cmd, 0, MSG_MAX_BUFFER);
	}
	char *pt = (char *)g_msg_cmd;
	// ESP_LOGI(TAG, "ble_empty_cmd_data %d", (int)pt);
	ESP_LOGI(TAG, "ble_empty_cmd_data %d", (int)pt);
	g_msg_cmd = 0;
}

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
			msg_open *open = (msg_open *)g_buffer_set;
			open->command = XV_GW_OPEN_LOG;
			memcpy(open->sn, g_device_list[i].sn, BLE_NAME_LEN);
			msg_set((msg_base *)open);
		}
	}
}

// 添加蓝牙设备信息
int add_ble_device(struct ble_devices_state_t *state, const char *name, const uint8_t *mac_address, int confidence)
{
	// 检查是否已达到最大设备数量
	if (state->num_devices >= MAX_DEVICES)
	{
		return -1; // 表示添加失败，超过了最大设备数量
	}

	// 检查名称的长度是否超过限制
	if (strlen(name) >= 20)
	{
		return -2; // 表示添加失败，名称长度超过限制
	}

	// 将设备信息添加到结构体数组中
	strcpy(state->devices[state->num_devices].name, name);
	memcpy(state->devices[state->num_devices].mac_address, mac_address, sizeof(int) * 6);
	state->devices[state->num_devices].confidence = confidence;

	// 增加设备数量
	state->num_devices++;

	return 0; // 表示添加成功
}

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
	memset((char *)&device->dev_cb, 0, sizeof(esp_ble_gap_cb_param_t));
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

void ble_devices_init()
{

	init_ble_devices_state(&my_ble_devices_state);

	// 添加设备信息
	uint8_t mac_address1[6] = {0xC7, 0x6A, 0xCD, 0x04, 0x1A, 0x80};
	uint8_t mac_address2[6] = {0xD0, 0x62, 0x2C, 0xCD, 0x8A, 0x94};

	int result1 = add_ble_device(&my_ble_devices_state, "Mi Smart Band 6", mac_address1, 0);
	int result2 = add_ble_device(&my_ble_devices_state, "Mi Smart Band 8", mac_address2, 0);

	if (result1 == 0)
	{
		ESP_LOGI(TAG, "Device 1 added successfully!");
	}
	else
	{
		ESP_LOGI(TAG, "Failed to add Device 1! Error code: %d", result1);
	}

	if (result2 == 0)
	{
		ESP_LOGI(TAG, "Device 2 added successfully!\n");
	}
	else
	{
		ESP_LOGI(TAG, "Failed to add Device 2! Error code: %d", result2);
	}
}

void esp_gap_cb(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param)
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
				for (uint8_t i = 0; i < MAX_DEVICES; i++)
				{
					if (strlen(my_ble_devices_state.devices[i].name) == adv_name_len && strncmp((char *)adv_name, my_ble_devices_state.devices[i].name, adv_name_len) == 0)
					{
						my_ble_devices_state.devices[i].rssi = scan_result->scan_rst.rssi < 0 ? ~(scan_result->scan_rst.rssi) : 0;
						my_ble_devices_state.devices[i].confidence = my_ble_devices_state.devices[i].rssi > 0 ? 100 : 0;

						// mqtt_send_device_info(my_ble_devices_state.devices[i].name, my_ble_devices_state.devices[i].confidence, my_ble_devices_state.devices[i].rssi);
						ESP_LOGI(TAG, "Device name: %s,Confidence: %d, RSSI: %d", my_ble_devices_state.devices[i].name, my_ble_devices_state.devices[i].confidence, my_ble_devices_state.devices[0].rssi);
					}
				}
				g_mi_band_rssi = scan_result->scan_rst.rssi;
				if (g_mi_band_rssi < 0)
				{
					g_mi_band_rssi = ~g_mi_band_rssi;
				}
			}

			if (adv_name != NULL && memcmp(adv_name, "TM", 2) == 0)
			{
				// printf("scaned drived name: %s", adv_name);

				if (strlen((char *)adv_name) > BLE_NAME_LEN)
				{
					adv_name += (strlen((char *)adv_name) - BLE_NAME_LEN);
					// printf("device_name more: %s", adv_name);
				}
				for (int i = 0; i < XV_LOCK_LIST_LENGTH; i++)
				{
					if (g_device_list[i].sn[0] && ble_device_check(&g_device_list[i], adv_name))
					{
						ble_device_set_dev(&g_device_list[i], scan_result);
						ESP_LOGI(TAG, "Device name: %s, RSSI: %d", adv_name, scan_result->scan_rst.rssi);
						// ESP_LOGI(TAG, "********************NowTimeStamp: %lld********************", nowTimeStamp());
						g_device_list[i].checked = true;
						g_device_list[i].rssi = scan_result->scan_rst.rssi;
						// lock have message to report
						if ((&scan_result->scan_rst.ble_adv[0])[0] == 0x07 && (&scan_result->scan_rst.ble_adv[0])[1] == 0xff)
						{
							g_device_list[i].has_data = true;
						}
						else
						{
							g_device_list[i].has_data = false;
						}
						g_device_list[i].online = true;
					}
					else
					{
						; // ESP_LOGI(TAG, "device_name not: %s", adv_name);
					}
				}
			}
			break;

		case ESP_GAP_SEARCH_INQ_CMPL_EVT:
		{

			char band[] = "Mi Smart Band 6";
			char rssi_str[5];
			int confid = 0;
			sprintf(rssi_str, "%ddBm", g_mi_band_rssi);
			// ssd1306_display_text_x3(&dev, 5, rssi_str, 5, false);
			ESP_LOGI(TAG, "Device name: %s, RSSI: %d", band, g_mi_band_rssi);
			// UPDATE SUB LOCK LIST/
			if (g_mi_band_rssi < 90 && g_mi_band_rssi > 0)
			{
				confid = 100;
			}
			else
			{
				confid = 0;
			}
			// UPDATE SUB LOCK LIST/
			if (json_start())
			{
				json_put_string("device_name", band);
				json_split();
				json_put_int("confidence", confid);

				json_end();
				char *buffer = json_buffer();
				esp_mqtt_client_publish(g_mqtt_client, g_topic_up, buffer, 0, QOS1, 0);
			}
			led_blink();
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

void esp_gattc_cb(esp_gattc_cb_event_t event, esp_gatt_if_t gattc_if, esp_ble_gattc_cb_param_t *param)
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

void gattc_profile_event_handler(esp_gattc_cb_event_t event, esp_gatt_if_t gattc_if, esp_ble_gattc_cb_param_t *param)
{
	esp_ble_gattc_cb_param_t *p_data = (esp_ble_gattc_cb_param_t *)param;
	p_data_gb = p_data;
	if (0 != gattc_if)
	{
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
			if (0 != g_msg_cmd)
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

// BLE conect timer config
esp_timer_handle_t timer_once_hanle = 0;
esp_timer_create_args_t test_once_arg = {
	.callback = &BLE_timeout_timer, // callback
	.arg = NULL,					// no args
	.name = "BLETIMER"				// timer name
};

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