#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "driver/gpio.h"
#include "errno.h"

#include <sys/time.h>
#include <time.h>

#include "sntp.h"
#include "ssd1306.h"

extern int g_mi_band_rssi;
extern SSD1306_t dev;
/**
 * @description: display for ssd1306
 * @param {*}
 * @return {*}
 */
void display_init()
{
	int i = 0;
	char rssi_str[5];

	spi_master_init(&dev, CONFIG_MOSI_GPIO, CONFIG_SCLK_GPIO, CONFIG_CS_GPIO, CONFIG_DC_GPIO, CONFIG_RESET_GPIO);
	ssd1306_init(&dev, 128, 64);
    ssd1306_contrast(&dev, 0xff);
	ssd1306_clear_screen(&dev, false);
	for(int i = 1; i < 6; i++)
	{
		ssd1306_display_text_x3(&dev, 1, "ETHAN", i, false);
		vTaskDelay(200 / portTICK_RATE_MS);
	}
	for(int i = 1; i < 6; i++)
	{
		ssd1306_display_text_x3(&dev, 4, "ESP32", i, false);
		vTaskDelay(200 / portTICK_RATE_MS);

	}
	// ssd1306_display_text_x3(&dev, 2, "ETHAN", 5, false);
	// ssd1306_display_text_x3(&dev, 5, "ESP32", 5, false);
    vTaskDelay(3000 / portTICK_RATE_MS);
	ssd1306_clear_screen(&dev, false);
	ssd1306_display_text(&dev, 1, "----------------", 16, false);
	ssd1306_display_text_x3(&dev, 2, "MRSSI", 5, false);
	ssd1306_display_text_x3(&dev, 5, "(T_T)", 5, false);

	ssd1306_clear_line(&dev, 1, false);
    while(1)
    {
		if ((i % 2) == 0)
		{
			ssd1306_display_text_x3(&dev, 2, "WRSSI", 5, false);
		}
		else
		{
			ssd1306_display_text_x3(&dev, 2, "MRSSI", 5, false);
		}
		if (i > 16)
		{
			ssd1306_clear_line(&dev, 1, false);
			if (!g_mi_band_rssi)
			{
				ssd1306_display_text_x3(&dev, 5, "(T_T)", 5, false);

			}
			else
			{
				sprintf(rssi_str, "<%d>", g_mi_band_rssi);
				ssd1306_display_text_x3(&dev, 5, rssi_str, 5, false);

			}
			i = 0;
		}
        vTaskDelay(500 / portTICK_PERIOD_MS);   /* 延时200ms*/
		ssd1306_display_text(&dev, 1, ">>>>>>>>>>>>>>>>", i++, false);
		nowTime();
    }
}