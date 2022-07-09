/*
 * @Author: Ethan 1781387847@qq.com
 * @Date: 2022-06-02 10:38:39
 * @LastEditors: Ethan 1781387847@qq.com
 * @LastEditTime: 2022-06-02 11:25:48
 * @FilePath: \Ble_Gateway\main\led.c
 * @Description: 
 * 
 * Copyright (c) 2022 by Ethan 1781387847@qq.com, All Rights Reserved. 
 */
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "driver/gpio.h"
#include "errno.h"

#include "led.h"
#include "ssd1306.h"

extern uint8_t g_led_flag;

void led_init(void)
{
    /* 定义一个gpio配置结构体 */
    gpio_config_t gpio_config_structure;

    /* 初始化gpio配置结构体*/ 
    gpio_config_structure.pin_bit_mask = (1ULL << GPIO_LED_NUM);/* 选择gpio2 */
    gpio_config_structure.mode = GPIO_MODE_OUTPUT;              /* 输出模式 */
    gpio_config_structure.pull_up_en = 0;                       /* 不上拉 */
    gpio_config_structure.pull_down_en = 0;                     /* 不下拉 */
    gpio_config_structure.intr_type = GPIO_PIN_INTR_DISABLE;    /* 禁止中断 */ 
    /* 根据设定参数初始化并使能 */  
	gpio_config(&gpio_config_structure);

    gpio_config_structure.pin_bit_mask = (1ULL << GPIO_LED_POWERED_NUM);/* 选择gpio2 */
    gpio_config_structure.mode = GPIO_MODE_OUTPUT;              /* 输出模式 */
    gpio_config_structure.pull_up_en = 0;                       /* 不上拉 */
    gpio_config_structure.pull_down_en = 0;                     /* 不下拉 */
    gpio_config_structure.intr_type = GPIO_PIN_INTR_DISABLE;    /* 禁止中断 */ 
    /* 根据设定参数初始化并使能 */  
	gpio_config(&gpio_config_structure);

    gpio_set_level(GPIO_LED_POWERED_NUM, 1);        /* 红灯默认熄灭 */
    gpio_set_level(GPIO_LED_NUM, 0);        /* 蓝灯默认熄灭 */
	
}


/**
 * @description: LED status, Extinguish\Always bright\Flickering
 * @param {*}
 * @return {*}
 */
void led_flicker()
{

    while(1)
    {
        if (g_led_flag == 0)
        {
            gpio_set_level(GPIO_LED_NUM, 0);        /* 熄灭 */
            vTaskDelay(100 / portTICK_PERIOD_MS);   /* 延时500ms*/
        }
        else if (g_led_flag == 1)
        {
            gpio_set_level(GPIO_LED_NUM, 1);        
            vTaskDelay(100 / portTICK_PERIOD_MS);   /* 延时500ms*/
            vTaskDelete(NULL);
        }
        else if (g_led_flag == 2)
        {
            gpio_set_level(GPIO_LED_NUM, 0);        /* 熄灭 */
            vTaskDelay(200 / portTICK_PERIOD_MS);   /* 延时500ms*/
            gpio_set_level(GPIO_LED_NUM, 1);        /* 点亮 */
            vTaskDelay(200 / portTICK_PERIOD_MS);   /* 延时500ms*/
        }
        else if (g_led_flag == 3)
        {
            gpio_set_level(GPIO_LED_NUM, 0);        /* 熄灭 */
            vTaskDelay(500 / portTICK_PERIOD_MS);   /* 延时500ms*/
            gpio_set_level(GPIO_LED_NUM, 1);        /* 点亮 */
            vTaskDelay(500 / portTICK_PERIOD_MS);   /* 延时500ms*/
        }
        else if (g_led_flag == 4)
        {
            gpio_set_level(GPIO_LED_NUM, 0);        /* 熄灭 */
            vTaskDelay(100 / portTICK_PERIOD_MS);   /* 延时500ms*/
            gpio_set_level(GPIO_LED_NUM, 1);        /* 点亮 */
            vTaskDelay(100 / portTICK_PERIOD_MS);   /* 延时500ms*/
        }
        else if (g_led_flag == 5)
        {
            gpio_set_level(GPIO_LED_NUM, 0);        /* 熄灭 */
            vTaskDelay(50 / portTICK_PERIOD_MS);   /* 延时500ms*/
            gpio_set_level(GPIO_LED_NUM, 1);        /* 点亮 */
            vTaskDelay(50 / portTICK_PERIOD_MS);   /* 延时500ms*/
        }
    }          

}