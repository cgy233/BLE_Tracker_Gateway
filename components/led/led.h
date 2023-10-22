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
#ifndef _LED_H_
#define _LED_H_

// WU
// #define GPIO_LED_NUM 27
// LUO
#define GPIO_LED_POWERED_NUM 32
#define GPIO_LED_NUM 33
#define GPIO_BLINK_LED_NUM 2

void led_init();
void led_on();
void led_off();
void led_flicker();
void led_blink(uint8_t count);

#endif