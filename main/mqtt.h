#ifndef __MQTT_H__
#define __MQTT_H__

#include "mqtt_client.h"
#include "msg_list.h"
#include "ble_manager.h"

// MQTT Parameter
#define HOST_NAME "cyupi.top"
#define HOST_PORT 1883
#define CLIENT_ID "ethanhomeesp32kit"
#define AIR_CMD 14 // Lock count
#define TOPIC "esp32/ethanhome/miband6"

void mqtt_send_device_info(char *name, uint8_t confidence, int8_t rssi);
void xv_mqtt_event_hanler(esp_mqtt_event_handle_t event);
void log_error_if_nonzero(const char *message, int error_code);
void mqtt_app_start(char *mac);
void mqtt_init();

#endif