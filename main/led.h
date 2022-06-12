/*
 * @Author: Ethan 1781387847@qq.com
 * @Date: 2022-06-02 10:38:29
 * @LastEditors: Ethan 1781387847@qq.com
 * @LastEditTime: 2022-06-02 10:54:15
 * @FilePath: \Ble_Gateway\main\led.h
 * @Description: 
 * 
 * Copyright (c) 2022 by Ethan 1781387847@qq.com, All Rights Reserved. 
 */
#ifndef __led__header__h__
#define __led__header__h__

// WU
// #define GPIO_LED_NUM 27
// LUO
#define GPIO_LED_POWERED_NUM 32
#define GPIO_LED_NUM 33

void led_init();
void led_flicker();

#endif