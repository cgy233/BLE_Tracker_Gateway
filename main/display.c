#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "driver/gpio.h"
#include "errno.h"

#include <sys/time.h>
#include <time.h>

#include "ssd1306.h"

extern SSD1306_t dev;
/**
 * @description: display for ssd1306
 * @param {*}
 * @return {*}
 */
void display_init()
{
	spi_master_init(&dev, CONFIG_MOSI_GPIO, CONFIG_SCLK_GPIO, CONFIG_CS_GPIO, CONFIG_DC_GPIO, CONFIG_RESET_GPIO);
	ssd1306_init(&dev, 128, 64);
    ssd1306_contrast(&dev, 0xff);
	ssd1306_clear_screen(&dev, false);
	ssd1306_display_text_x3(&dev, 1, "ETHAN", 5, false);
	ssd1306_display_text_x3(&dev, 5, "ESP32", 5, false);
    vTaskDelay(3000 / portTICK_RATE_MS);
	ssd1306_clear_screen(&dev, false);
	ssd1306_display_text_x3(&dev, 2, "|RSSI", 5, false);
    ssd1306_display_text_x3(&dev, 5, "|000|", 5, false);

    // ESP_LOGI(TAG, "The current date/time in Shanghai is: %s", strftime_buf);

    while(1)
    {
        vTaskDelay(200 / portTICK_PERIOD_MS);   /* 延时200ms*/
		// localtime_r(&now, &timeinfo);
		// time(&now);
		// // Set timezone to China Standard Time
		// setenv("TZ", "CST-8", 1);
		// tzset();
		// strftime(strftime_buf, sizeof(strftime_buf), "%c", &timeinfo);
		// ssd1306_display_text(&dev, 0, strftime_buf+4, sizeof(strftime_buf), false);
    }
}