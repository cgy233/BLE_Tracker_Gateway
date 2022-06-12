#include "nvs.h"
#include "nvs_flash.h"

#include "esp_system.h"
#include "esp_ota_ops.h"
#include "esp_http_client.h"
#include "esp_flash_partitions.h"
#include "esp_partition.h"

#include "lwip/sockets.h"
#include "lwip/dns.h"
#include "lwip/netdb.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "freertos/event_groups.h"
#include "mqtt_client.h"
#include "driver/gpio.h"
#include "errno.h"
#include "ota.h"

#define XV_GW_OTA 2
#define QOS1 1

// OTA
#define BUFFSIZE 1024
#define HASH_LEN 32 /* SHA-256 digest length */
#define OTA_URL_SIZE 256

extern esp_mqtt_client_handle_t g_mqtt_client; // MQTT client handle
extern char g_topic_up[32];
extern uint8_t g_version;

static char ota_write_data[BUFFSIZE + 1] = { 0 }; // ota data write buffer ready to write to the flash
static void http_cleanup(esp_http_client_handle_t client)
{
    esp_http_client_close(client);
    esp_http_client_cleanup(client);
}

static void __attribute__((noreturn)) task_fatal_error(void)
{
    printf("Exiting task due to fatal error...\n");
    vTaskDelete(NULL);

    while (1) {
        ;
    }
}

static void print_sha256 (const uint8_t *image_hash, const char *label)
{
    char hash_print[HASH_LEN * 2 + 1];
    hash_print[HASH_LEN * 2] = 0;
    for (int i = 0; i < HASH_LEN; ++i) {
        sprintf(&hash_print[i * 2], "%02x", image_hash[i]);
    }
    printf("%s: %s\n", label, hash_print);
}

/**
 * @description: Aerial upgrade
 * @param {void} *pvParameter
 * @return {*}
 */
void ota_task(void *pvParameter)
{
    esp_err_t err;
    /* update handle : set by esp_ota_begin(), must be freed via esp_ota_end() */
    esp_ota_handle_t update_handle = 0 ;
    const esp_partition_t *update_partition = NULL;

    printf("Starting OTA...\n");

    const esp_partition_t *configured = esp_ota_get_boot_partition();
    const esp_partition_t *running = esp_ota_get_running_partition();

    if (configured != running) {
        printf("Configured OTA boot partition at offset 0x%08x, but running from offset 0x%08x\n", configured->address, running->address);
        printf("(This can happen if either the OTA boot data or preferred boot image become corrupted somehow.)\n");
    }
    printf("Running partition type %d subtype %d (offset 0x%08x)\n", running->type, running->subtype, running->address);

    esp_http_client_config_t config = {
        .url = pvParameter,
        .timeout_ms = 12000,
        .keep_alive_enable = true,
    };

#ifdef CONFIG_EXAMPLE_SKIP_COMMON_NAME_CHECK
    config.skip_cert_common_name_check = true;
#endif

    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (client == NULL) {
        printf("Failed to initialise HTTP connection\n");
        task_fatal_error();
    }
    err = esp_http_client_open(client, 0);
    if (err != ESP_OK) {
        printf("Failed to open HTTP connection: %s\n", esp_err_to_name(err));
        esp_http_client_cleanup(client);
        task_fatal_error();
    }
    esp_http_client_fetch_headers(client);

    update_partition = esp_ota_get_next_update_partition(NULL);
    assert(update_partition != NULL);
    printf("Writing to partition subtype %d at offset 0x%x\n", update_partition->subtype, update_partition->address);

    int binary_file_length = 0;
    /*deal with all receive packet*/
    bool image_header_was_checked = false;
    while (1) {
        int data_read = esp_http_client_read(client, ota_write_data, BUFFSIZE);
        if (data_read < 0) {
            printf("Error: SSL data read error\n");
            http_cleanup(client);
            task_fatal_error();
        } else if (data_read > 0) {
            if (image_header_was_checked == false) {
                esp_app_desc_t new_app_info;
                if (data_read > sizeof(esp_image_header_t) + sizeof(esp_image_segment_header_t) + sizeof(esp_app_desc_t)) {
                    // check current version with downloading
                    memcpy(&new_app_info, &ota_write_data[sizeof(esp_image_header_t) + sizeof(esp_image_segment_header_t)], sizeof(esp_app_desc_t));
                    printf("New firmware version: %s", new_app_info.version);

                    esp_app_desc_t running_app_info;
                    if (esp_ota_get_partition_description(running, &running_app_info) == ESP_OK) {
                        printf("Running firmware version: %s", running_app_info.version);
                    }

                    const esp_partition_t* last_invalid_app = esp_ota_get_last_invalid_partition();
                    esp_app_desc_t invalid_app_info;
                    if (esp_ota_get_partition_description(last_invalid_app, &invalid_app_info) == ESP_OK) {
                        printf("Last invalid firmware version: %s", invalid_app_info.version);
                    }

                    // check current version with last invalid partition
                    if (last_invalid_app != NULL) {
                        if (memcmp(invalid_app_info.version, new_app_info.version, sizeof(new_app_info.version)) == 0) {
                            printf("New version is the same as invalid version.");
                            printf("Previously, there was an attempt to launch the firmware with %s version, but it failed.", invalid_app_info.version);
                            printf("The firmware has been rolled back to the previous version.");
                            http_cleanup(client);
                            vTaskDelete(NULL);
                            //response mqtt
                        }
                    }
                    if (memcmp(new_app_info.version, running_app_info.version, sizeof(new_app_info.version)) == 0) {
                        printf("Current running version is the same as a new. We will not continue the update.");
                        http_cleanup(client);
                        vTaskDelete(NULL);
                        //response mqtt
                    }
                    image_header_was_checked = true;

                    err = esp_ota_begin(update_partition, OTA_WITH_SEQUENTIAL_WRITES, &update_handle);
                    if (err != ESP_OK) {
                        printf("esp_ota_begin failed (%s)", esp_err_to_name(err));
                        http_cleanup(client);
                        esp_ota_abort(update_handle);
                        task_fatal_error();
                    }
                    printf("esp_ota_begin succeeded");
					char buffer[20] = "{\"command\": 6}";
					printf("Publish Start OTA Done: %s\n", buffer);
					esp_mqtt_client_publish(g_mqtt_client, g_topic_up, buffer, 0, QOS1, 0);
                } else {
                    printf("received package is not fit len\n");
                    http_cleanup(client);
                    esp_ota_abort(update_handle);
                    task_fatal_error();
                }
            }
            err = esp_ota_write( update_handle, (const void *)ota_write_data, data_read);
            if (err != ESP_OK) {
                http_cleanup(client);
                esp_ota_abort(update_handle);
                task_fatal_error();
            }
            binary_file_length += data_read;
            //printf("Written image length %d\n", binary_file_length);
        } else if (data_read == 0) {
           /*
            * As esp_http_client_read never returns negative error code, we rely on
            * `errno` to check for underlying transport connectivity closure if any
            */
            if (errno == ECONNRESET || errno == ENOTCONN) {
                printf("Connection closed, errno = %d\n", errno);
                break;
            }
            if (esp_http_client_is_complete_data_received(client) == true) {
                printf("Connection closed\n");
                break;
            }
        }
    }
    printf("Total Write binary data length: %d\n", binary_file_length);
    if (esp_http_client_is_complete_data_received(client) != true) {
        printf("Error in receiving complete file\n");
        http_cleanup(client);
        esp_ota_abort(update_handle);
        task_fatal_error();
    }

    err = esp_ota_end(update_handle);
    if (err != ESP_OK) {
        if (err == ESP_ERR_OTA_VALIDATE_FAILED) {
            printf("Image validation failed, image is corrupted\n");
        } else {
            printf("esp_ota_end failed (%s)!\n", esp_err_to_name(err));
        }
        http_cleanup(client);
        task_fatal_error();
    }

    err = esp_ota_set_boot_partition(update_partition);
    if (err != ESP_OK) {
        printf("esp_ota_set_boot_partition failed (%s)!", esp_err_to_name(err));
        http_cleanup(client);
        task_fatal_error();
    }
    printf("Prepare to restart system!\n");
    printf("OTA Done.\n");
    printf("Prepare to restart system!\n");
    esp_restart();
    return ;
}

static bool diagnostic(void)
{
    gpio_config_t io_conf;
    io_conf.intr_type    = GPIO_INTR_DISABLE;
    io_conf.mode         = GPIO_MODE_INPUT;
    // io_conf.pin_bit_mask = (1ULL << CONFIG_EXAMPLE_GPIO_DIAGNOSTIC);
    io_conf.pin_bit_mask = (1ULL << 4);
    io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
    io_conf.pull_up_en   = GPIO_PULLUP_ENABLE;
    gpio_config(&io_conf);

    printf("Diagnostics (5 sec)...\n");
    vTaskDelay(5000 / portTICK_PERIOD_MS);

    // bool diagnostic_is_ok = gpio_get_level(CONFIG_EXAMPLE_GPIO_DIAGNOSTIC);
    bool diagnostic_is_ok = gpio_get_level(4);

    // gpio_reset_pin(CONFIG_EXAMPLE_GPIO_DIAGNOSTIC);
    gpio_reset_pin(4);
    return diagnostic_is_ok;
}

/**
 * @description: OTA initialization
 * @param {*}
 * @return {*}
 */
void ota_init()
{
    uint8_t sha_256[HASH_LEN] = { 0 };
    esp_partition_t partition;

    // get sha256 digest for the partition table
    partition.address   = ESP_PARTITION_TABLE_OFFSET;
    partition.size      = ESP_PARTITION_TABLE_MAX_LEN;
    partition.type      = ESP_PARTITION_TYPE_DATA;
    esp_partition_get_sha256(&partition, sha_256);
    print_sha256(sha_256, "SHA-256 for the partition table: ");

    // get sha256 digest for bootloader
    partition.address   = ESP_BOOTLOADER_OFFSET;
    partition.size      = ESP_PARTITION_TABLE_OFFSET;
    partition.type      = ESP_PARTITION_TYPE_APP;
    esp_partition_get_sha256(&partition, sha_256);
    print_sha256(sha_256, "SHA-256 for bootloader: ");

    // get sha256 digest for running partition
    esp_partition_get_sha256(esp_ota_get_running_partition(), sha_256);
    print_sha256(sha_256, "SHA-256 for current firmware: ");

    const esp_partition_t *running = esp_ota_get_running_partition();
    esp_ota_img_states_t ota_state;
    if (esp_ota_get_state_partition(running, &ota_state) == ESP_OK) {
        if (ota_state == ESP_OTA_IMG_PENDING_VERIFY) {
            // run diagnostic function ...
            bool diagnostic_is_ok = diagnostic();
            if (diagnostic_is_ok) {
                printf("Diagnostics completed successfully! Continuing execution ...\n");
                esp_ota_mark_app_valid_cancel_rollback();
            } else {
                printf("Diagnostics failed! Start rollback to the previous version ...\n");
                esp_ota_mark_app_invalid_rollback_and_reboot();
            }
        }
    }
}

void check_firmware_version()
{
    const esp_partition_t *running = esp_ota_get_running_partition();

    esp_app_desc_t running_app_info;
    if (esp_ota_get_partition_description(running, &running_app_info) == ESP_OK) {
        g_version = atoi(running_app_info.version);
        printf("**************************************************\n");
        printf("**************************************************\n");
        printf("*************FIRMWARE VERSION: %d******************\n", g_version);
        printf("**************************************************\n");
        printf("**************************************************\n");
    }
}