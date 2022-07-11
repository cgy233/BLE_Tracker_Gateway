/*
 * @Author: Ethan 1781387847@qq.com
 * @Date: 2022-06-02 11:10:27
 * @LastEditors: error: git config user.name && git config user.email & please set dead value or install git
 * @LastEditTime: 2022-06-12 16:57:24
 * @FilePath: \BLE_Tracker_Gateway\main\wifi_smartconfig.h
 * @Description: 
 * 
 * Copyright (c) 2022 by Ethan 1781387847@qq.com, All Rights Reserved. 
 */
#ifndef __wifi_smartconfig__header__h__
#define __wifi_smartconfig__header__h__

/* 宏定义WiFi更新标识码、WiFi名称和密码 */
#define MY_WIFI_UPDATE  4096        
#define MY_WIFI_SSID    "jnwap"
#define MY_WIFI_PASSWD  "jianeng123"

/* 宏定义WiFi连接事件标志位、连接失败标志位及智能配网标志位 */
#define WIFI_CONNECTED_BIT  BIT0
#define WIFI_FAIL_BIT       BIT1
#define SMART_CONFIG_BIT    BIT2

void wifi_init_sta();
void wifi_smartconfig_check();
void smartconfig_init_start();
#endif