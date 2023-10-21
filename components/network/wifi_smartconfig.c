#include "nvs.h"
#include "nvs_flash.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_system.h"
#include "esp_smartconfig.h"
#include "esp_http_client.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "wifi_smartconfig.h"

#ifndef TAG
#define TAG "WIFI"
#endif


extern uint8_t g_led_flag;
static EventGroupHandle_t wifi_event_group_handler; // WiFi连接事件标志组句柄

void wifi_smartconfig_check(void)
{
    // SmartConfig INIT
    /* 定义一个NVS操作句柄 */
    nvs_handle wificfg_nvs_handler;
    /* 打开一个NVS命名空间 */
    ESP_ERROR_CHECK(nvs_open("WiFi_cfg", NVS_READWRITE, &wificfg_nvs_handler));
    uint32_t wifi_update = 0;
    nvs_get_u32(wificfg_nvs_handler,"wifi_update",&wifi_update);
    if(MY_WIFI_UPDATE == wifi_update )
        ESP_LOGI(TAG, "wifi_cfg needn't to update. ");
    else
    {
        ESP_LOGI(TAG, "wifi_cfg update now... ");
        ESP_ERROR_CHECK( nvs_set_str(wificfg_nvs_handler,"wifi_ssid",MY_WIFI_SSID) );
        ESP_ERROR_CHECK( nvs_set_str(wificfg_nvs_handler,"wifi_passwd",MY_WIFI_PASSWD) ); 
        ESP_ERROR_CHECK( nvs_set_u32(wificfg_nvs_handler,"wifi_update",MY_WIFI_UPDATE) );
        ESP_LOGI(TAG, "wifi_cfg update ok. ");
    }
    ESP_ERROR_CHECK( nvs_commit(wificfg_nvs_handler) ); /* 提交 */
    nvs_close(wificfg_nvs_handler);                     /* 关闭 */
}

/**
 * @description: SmartConfig 智能配网
 * @param {void*} arg
 * @param {esp_event_base_t} event_base
 * @param {int32_t} event_id
 * @param {void*} event_data
 * @return {*}
 */
static void smartconfig_event_handler(void* arg, esp_event_base_t event_base,int32_t event_id, void* event_data)
{
    static int retry_num = 0;           /* 记录wifi重连次数 */
    /* 系统事件为WiFi事件 */
    if (event_base == WIFI_EVENT) 
    {
        if(event_id == WIFI_EVENT_STA_START)    /* 事件id为STA开始 */
            esp_wifi_connect();
        else if (event_id == WIFI_EVENT_STA_DISCONNECTED) /* 事件id为失去STA连接 */ 
        {
            if (0 == g_led_flag)
            {
                esp_wifi_connect();
                retry_num++;
                ESP_LOGI(TAG, "retry to connect to the AP %d times. ",retry_num);
                if (retry_num > 30)  /* WiFi重连次数大于10 */
                {
                    /* 将WiFi连接事件标志组的WiFi连接失败事件位置1 */
                    // xEventGroupSetBits(wifi_event_group_handler, WIFI_FAIL_BIT);
                    retry_num = 0;
                }
                /* 清除WiFi连接成功标志位 */
                xEventGroupClearBits(wifi_event_group_handler, WIFI_CONNECTED_BIT);

            }
        }
    } 
    /* 系统事件为ip地址事件，且事件id为成功获取ip地址 */
    else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) 
    {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data; /* 获取IP地址信息*/
        ESP_LOGI(TAG, "got ip:%d.%d.%d.%d " , IP2STR(&event->ip_info.ip));  /* 打印ip地址*/
        retry_num = 0;                                              /* WiFi重连次数清零 */
        /* 将WiFi连接事件标志组的WiFi连接成功事件位置1 */
        xEventGroupSetBits(wifi_event_group_handler, WIFI_CONNECTED_BIT);
    }
    /* 系统事件为智能配网事件 */
    else if (event_base == SC_EVENT)
    {
        if(event_id == SC_EVENT_SCAN_DONE )             /* 开始扫描智能配网设备端 */
        {
            ESP_LOGI(TAG, "Scan done");
            g_led_flag = 3;
        }
        else if(event_id == SC_EVENT_FOUND_CHANNEL)     /* 得到了智能配网通道 */
        {
            ESP_LOGI(TAG, "Found channel ");
            g_led_flag = 4;
        }
        else if(event_id == SC_EVENT_GOT_SSID_PSWD)     /* 得到了智能配网设备提供的ssid和password */
        {
            ESP_LOGI(TAG, "smartconfig got SSID and password");
            g_led_flag = 5;
            /* 获取智能配网设备端提供的数据信息 */
            smartconfig_event_got_ssid_pswd_t *evt = (smartconfig_event_got_ssid_pswd_t *)event_data;
            /* 定义WiFi配置结构体和用了结收ssid和password的数组 */
            wifi_config_t wifi_config;
            char ssid[32] = { 0 };
            char password[64] = { 0 };

            bzero(&wifi_config, sizeof(wifi_config_t)); /* 将结构体数据清零 */
            /* 将智能配网设备发送来的WiFi的ssid、password及MAC地址复制到wifi_config */
            memcpy(wifi_config.sta.ssid, evt->ssid, sizeof(wifi_config.sta.ssid));
            memcpy(wifi_config.sta.password, evt->password, sizeof(wifi_config.sta.password));
            wifi_config.sta.bssid_set = evt->bssid_set;
            if (wifi_config.sta.bssid_set == true) {
                memcpy(wifi_config.sta.bssid, evt->bssid, sizeof(wifi_config.sta.bssid));
            }
            
            /* 打印WiFi名称和密码 */
            memcpy(ssid, evt->ssid, sizeof(evt->ssid));
            memcpy(password, evt->password, sizeof(evt->password));
            ESP_LOGI(TAG, "SSID:%s ", ssid);
            ESP_LOGI(TAG, "PASSWORD:%s ", password);

            /* 将得到的WiFi名称和密码存入NVS*/
            nvs_handle wificfg_nvs_handler;
            ESP_ERROR_CHECK( nvs_open("WiFi_cfg", NVS_READWRITE, &wificfg_nvs_handler) );
            ESP_ERROR_CHECK( nvs_set_str(wificfg_nvs_handler,"wifi_ssid",ssid) );
            ESP_ERROR_CHECK( nvs_set_str(wificfg_nvs_handler,"wifi_passwd",password) );
            ESP_ERROR_CHECK( nvs_commit(wificfg_nvs_handler) ); /* 提交 */
            nvs_close(wificfg_nvs_handler);                     /* 关闭 */ 
            ESP_LOGI(TAG, "smartconfig save wifi_cfg to NVS .");

            /* 根据得到的WiFi名称和密码连接WiFi*/
            ESP_ERROR_CHECK( esp_wifi_disconnect() );
            ESP_ERROR_CHECK( esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config) );
            ESP_ERROR_CHECK( esp_wifi_connect() );
        }
        else if (event_id == SC_EVENT_SEND_ACK_DONE)    /* 智能配网成功，已经给智能配网设备发送应答*/
        {
            xEventGroupSetBits(wifi_event_group_handler, SMART_CONFIG_BIT);
        }
    }
}

/**
 * @description: 智能配网功能初始化
 * @param {*}
 * @return {*}
 */
void smartconfig_init_start(void)
{
    /* 设置智能配网类型为 AirKiss */
    ESP_ERROR_CHECK( esp_smartconfig_set_type(SC_TYPE_ESPTOUCH) );
    /* 通过SMARTCONFIG_START_CONFIG_DEFAULT宏 来获取一个默认的smartconfig配置参数结构体变量*/
    smartconfig_start_config_t cfg = SMARTCONFIG_START_CONFIG_DEFAULT();
    /* 开始智能配网 */
    ESP_ERROR_CHECK( esp_smartconfig_start(&cfg) );
    ESP_LOGI(TAG, "smartconfig start ....... ");
    g_led_flag = 2;

    /* 使用事件标志组等待连接建立(WIFI_CONNECTED_BIT)或连接失败(WIFI_FAIL_BIT)事件 */
    EventBits_t uxBits;  /* 定义一个事件位变量来接收事件标志组等待函数的返回值 */
    /* 等待事件标志组，退出前清除设置的事件标志，任意置1就会返回*/
    uxBits = xEventGroupWaitBits(wifi_event_group_handler, WIFI_CONNECTED_BIT | SMART_CONFIG_BIT, 
                                    true, false, portMAX_DELAY); 
    if(uxBits & WIFI_CONNECTED_BIT) 
    {
        ESP_LOGI(TAG, "WiFi Connected to ap ok. ");
        esp_smartconfig_stop(); /* 关闭智能配网 */
    }
    if(uxBits & SMART_CONFIG_BIT) 
    {
        ESP_LOGI(TAG, "smartconfig over ");
        esp_smartconfig_stop(); /* 关闭智能配网 */
    }
}

/**
 * @description: 初始化WIFI模块，获取WIFI的连接状态
 * @param {*}
 * @return {*}
 */
void wifi_init_sta(void)
{
    /* 创建一个事件标志组 */
    wifi_event_group_handler = xEventGroupCreate();

    /* 初始化底层TCP/IP堆栈。在应用程序启动时，应该调用此函数一次。*/
    ESP_ERROR_CHECK(esp_netif_init());

    /* 创建默认事件循环,*/
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    /* 创建一个默认的WIFI-STA网络接口，如果初始化错误，此API将中止。*/
    esp_netif_create_default_wifi_sta();				

    /* 使用WIFI_INIT_CONFIG_DEFAULT() 来获取一个默认的wifi配置参数结构体变量*/
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    /* 根据cfg参数初始化wifi连接所需要的资源 */
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

	/* 将事件处理程序注册到系统默认事件循环，分别是WiFi事件、IP地址事件及smartconfig事件 */
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &smartconfig_event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &smartconfig_event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(SC_EVENT, ESP_EVENT_ANY_ID, &smartconfig_event_handler, NULL));

    nvs_handle wificfg_nvs_handler; /* 定义一个NVS操作句柄 */
    char wifi_ssid[32] = { 0 };     /* 定义一个数组用来存储ssid*/
    char wifi_passwd[64] = { 0 };   /* 定义一个数组用来存储passwd */
    size_t len;
    /* 打开一个NVS命名空间 */
    ESP_ERROR_CHECK( nvs_open("WiFi_cfg", NVS_READWRITE, &wificfg_nvs_handler) );
    len = sizeof(wifi_ssid);    /* 从NVS中获取ssid */
    ESP_ERROR_CHECK( nvs_get_str(wificfg_nvs_handler,"wifi_ssid",wifi_ssid,&len) );
    len = sizeof(wifi_passwd);      /* 从NVS中获取ssid */
    ESP_ERROR_CHECK( nvs_get_str(wificfg_nvs_handler,"wifi_passwd",wifi_passwd,&len) );
    ESP_ERROR_CHECK( nvs_commit(wificfg_nvs_handler) ); /* 提交 */
    nvs_close(wificfg_nvs_handler);                     /* 关闭 */
    /* 设置WiFi连接的ssid和password参数 */
    wifi_config_t wifi_config;
    bzero(&wifi_config, sizeof(wifi_config_t)); /* 将结构体数据清零 */
    memcpy(wifi_config.sta.ssid, wifi_ssid, sizeof(wifi_config.sta.ssid));
    memcpy(wifi_config.sta.password, wifi_passwd, sizeof(wifi_config.sta.password));

    /* 设置WiFi的工作模式为 STA */
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA) );
    /* 设置WiFi连接的参数，主要是ssid和password */
    ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config));
    /* 启动WiFi连接 */
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "wifi_init_sta finished. ");

    /* 使用事件标志组等待连接建立(WIFI_CONNECTED_BIT)或连接失败(WIFI_FAIL_BIT)事件 */
    EventBits_t bits;  /* 定义一个事件位变量来接收事件标志组等待函数的返回值 */ 
    bits = xEventGroupWaitBits( wifi_event_group_handler,	        /* 需要等待的事件标志组的句柄 */
                                WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,	/* 需要等待的事件位 */
                                pdFALSE,    /* 为pdFALSE时，在退出此函数之前所设置的这些事件位不变，为pdFALSE则清零*/ 
                                pdFALSE,    /* 为pdFALSE时，设置的这些事件位任意一个置1就会返回，为pdFALSE则需全为1才返回 */
                                portMAX_DELAY);	                    /* 设置为最长阻塞等待时间，单位为时钟节拍 */

    /* 根据事件标志组等待函数的返回值获取WiFi连接状态 */
    if (bits & WIFI_CONNECTED_BIT)  /* WiFi连接成功事件 */
	{
        ESP_LOGI(TAG, "connected to ap %s OK ",wifi_ssid);
        ESP_LOGI(TAG, "********************Wifi SSID: %s ********************", wifi_ssid);
        ESP_LOGI(TAG, "********************Wifi PASSWD: %s ********************", wifi_passwd);
        vEventGroupDelete(wifi_event_group_handler);    /* 删除WiFi连接事件标志组，WiFi连接成功后不再需要 */
    } 
	else if (bits & WIFI_FAIL_BIT) /* WiFi连接失败事件 */
	{
        ESP_LOGI(TAG, "Failed to connect to ap %s. ",wifi_ssid);
        smartconfig_init_start();   /* 开启智能配网 */
    } 
	else 
    {
        ESP_LOGI(TAG, "UNEXPECTED EVENT");  /* 没有等待到事件 */
        smartconfig_init_start();   /* 开启智能配网 */
    }
}