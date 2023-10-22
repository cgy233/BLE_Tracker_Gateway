#include "esp_log.h"
#include "esp_system.h"
#include "esp_event.h"
#include "esp_task_wdt.h"

#include "freertos/FreeRTOS.h"
#include "soc/rtc_wdt.h"

#include "mqtt.h"
#include "json.h"
#include "led.h"

extern esp_mqtt_client_handle_t g_mqtt_client; // MQTT client handle
extern uint8_t g_led_flag;			 // led status flag
extern uint8_t g_version; // Firmware version
extern uint8_t g_buf_send[32];																							  // BLE send data

char g_topic_up[32] = "esp32/ethanhome/miband6";

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

/**
 * @description: MQTT through the platform to get data for processing
 * @param {esp_mqtt_event_handle_t} event
 * @return {*}
 */
void xv_mqtt_event_hanler(esp_mqtt_event_handle_t event)
{
	// Feed dog
	#if 0
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
	#endif
}

/**
 * @description: MQTT ERROR CALLBACK
 * @param {char} *message
 * @param {int} error_code
 * @return {*}
 */
void log_error_if_nonzero(const char *message, int error_code)
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
void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data)
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
void mqtt_app_start(char *mac)
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
