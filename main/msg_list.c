#include "msg_list.h"

extern short g_scan_max;
extern char g_buffer_set[MSG_MAX_BUFFER];
extern char g_buffer_list[MSG_MAX_COUNT][MSG_MAX_BUFFER];
extern int g_msg_last;

void msg_set(msg_base *msg)
{
	g_scan_max = 3;
	for (int i = 0; i < MSG_MAX_COUNT; i++)
	{
		char *data = g_buffer_list[i];
		data += 1;
		if (0 == *data)
		{
			/// ESP_LOGI(TAG, "22222222 %d, %s", i, data+1);
			memcpy(g_buffer_list[i], (char *)msg, MSG_MAX_BUFFER);
			return;
		}
		else
		{
			// ESP_LOGI(TAG, "yyyyyyy %d- %s", i, data);
		}
	}
	// ESP_LOGI(TAG, "rrrrrr %d", g_msg_last);
	memcpy(g_buffer_list[g_msg_last], (char *)msg, MSG_MAX_BUFFER);
	++g_msg_last;
	if (g_msg_last >= MSG_MAX_COUNT)
	{
		g_msg_last = 0;
	}
}
/**
 * @description: Remove the command from the queue
 * @param {*}
 * @return {*}
 */
msg_base *msg_get()
{
	for (int i = 0; i < MSG_MAX_COUNT; i++)
	{
		char *data = g_buffer_list[i];
		data += 1;
		// ESP_LOGI(TAG, "msg_get %d, %s", i, data);
		if (*data != 0)
		{
			return (msg_base *)g_buffer_list[i];
		}
	}
	return 0;
}