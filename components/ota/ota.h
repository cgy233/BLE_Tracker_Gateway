#ifndef _OTA_H_
#define _OTA_H_

void check_firmware_version();
void ota_init();
void ota_task(void *pvParameter);


#endif