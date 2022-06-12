#ifndef __ota__header__h__
#define __ota__header__h__

void check_firmware_version();
void ota_init();
void ota_task(void *pvParameter);


#endif