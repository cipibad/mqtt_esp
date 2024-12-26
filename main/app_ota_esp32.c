#include "esp_system.h"
#if defined(CONFIG_MQTT_OTA) && defined(CONFIG_TARGET_DEVICE_ESP32)
#include "esp_log.h"
#include "esp_ota_ops.h"
#include "esp_http_client.h"
#include "esp_flash_partitions.h"
#include "esp_partition.h"

#include <string.h>

#include "esp_wifi.h"

#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/queue.h"

#include "cJSON.h"

#include "app_main.h"
#include "app_ota.h"
#include "app_publish_data.h"

static const char *TAG = "MQTTS_OTA";

extern QueueHandle_t otaQueue;

extern const uint8_t server_cert_pem_start[] asm("_binary_cert_bundle_pem_start");

#define BUFFSIZE 1024
/*an ota data write buffer ready to write to the flash*/
static char ota_write_data[BUFFSIZE + 1] = { 0 };

void publish_ota_data(int status)
{
  const char * topic = CONFIG_DEVICE_TYPE "/" CONFIG_CLIENT_ID "/evt/ota";
  char data[256];
  memset(data,0,256);

  sprintf(data, "{\"status\":%d}", status);

  publish_persistent_data(topic, data);
}

static void http_cleanup(esp_http_client_handle_t client)
{
    esp_http_client_close(client);
    esp_http_client_cleanup(client);
}

void handle_ota_update_task(void* pvParameters)
{

  esp_err_t err;
  /* update handle : set by esp_ota_begin(), must be freed via esp_ota_end() */
  esp_ota_handle_t update_handle = 0 ;
  const esp_partition_t *update_partition = NULL;

  ESP_LOGI(TAG, "Starting OTA example...");

  const esp_partition_t *configured = esp_ota_get_boot_partition();
  const esp_partition_t *running = esp_ota_get_running_partition();

  if (configured != running) {
    ESP_LOGW(TAG, "Configured OTA boot partition at offset 0x%08x, but running from offset 0x%08x",
             configured->address, running->address);
    ESP_LOGW(TAG, "(This can happen if either the OTA boot data or preferred boot image become corrupted somehow.)");
  }
  ESP_LOGI(TAG, "Running partition type %d subtype %d (offset 0x%08x)",
           running->type, running->subtype, running->address);
  struct OtaMessage o;
  char * url = "https://sw.iot.cipex.ro:8911/" CONFIG_CLIENT_ID ".bin";

  while(1) {
    if( xQueueReceive( otaQueue, &o , portMAX_DELAY) )
      {

        /* esp_wifi_stop(); */
        /* vTaskDelay(60000 / portTICK_PERIOD_MS); */
        /* esp_wifi_start(); */
        /* xEventGroupWaitBits(wifi_event_group, CONNECTED_BIT, false, true, portMAX_DELAY) */;
        publish_ota_data(OTA_ONGOING);

        esp_http_client_config_t config = {
          .url = url,
          .cert_pem = (char *)server_cert_pem_start,
        };
        esp_http_client_handle_t client = esp_http_client_init(&config);
        if (client == NULL) {
          ESP_LOGE(TAG, "Failed to initialise HTTP connection");
          ESP_LOGE(TAG, "Firmware Upgrade Failed");
          publish_ota_data(OTA_FAILED);
          continue;
        }
        err = esp_http_client_open(client, 0);
        if (err != ESP_OK) {
          ESP_LOGE(TAG, "Failed to open HTTP connection: %s", esp_err_to_name(err));
          esp_http_client_cleanup(client);
          ESP_LOGE(TAG, "Firmware Upgrade Failed");
          publish_ota_data(OTA_FAILED);
          continue;
        }
        esp_http_client_fetch_headers(client);

        update_partition = esp_ota_get_next_update_partition(NULL);
        ESP_LOGI(TAG, "Writing to partition subtype %d at offset 0x%x",
                 update_partition->subtype, update_partition->address);
        assert(update_partition != NULL);

        err = esp_ota_begin(update_partition, OTA_SIZE_UNKNOWN, &update_handle);
        if (err != ESP_OK) {
          ESP_LOGE(TAG, "esp_ota_begin failed (%s)", esp_err_to_name(err));
          http_cleanup(client);
          ESP_LOGE(TAG, "Firmware Upgrade Failed");
          publish_ota_data(OTA_FAILED);
          continue;
        }
        ESP_LOGI(TAG, "esp_ota_begin succeeded");

        int binary_file_length = 0;
        /*deal with all receive packet*/
        int failed=false;
        while (!failed) {
          int data_read = esp_http_client_read(client, ota_write_data, BUFFSIZE);
          if (data_read < 0) {
            ESP_LOGE(TAG, "Error: SSL data read error");
            failed=true;
            break;
          } else if (data_read > 0) {
            err = esp_ota_write( update_handle, (const void *)ota_write_data, data_read);
            if (err != ESP_OK) {
              failed=true;
              break;
            }
            binary_file_length += data_read;
            ESP_LOGD(TAG, "Written image length %d", binary_file_length);
          } else if (data_read == 0) {
            ESP_LOGI(TAG, "Connection closed,all data received");
            break;
          }
        }
        if(failed) {
          http_cleanup(client);
          ESP_LOGE(TAG, "Firmware Upgrade Failed");
          publish_ota_data(OTA_FAILED);
          continue;
        }

        ESP_LOGI(TAG, "Total Write binary data length : %d", binary_file_length);

        if (esp_ota_end(update_handle) != ESP_OK) {
          ESP_LOGE(TAG, "esp_ota_end failed!");
          http_cleanup(client);
          ESP_LOGE(TAG, "Firmware Upgrade Failed");
          publish_ota_data(OTA_FAILED);
          continue;
        }

        if (esp_partition_check_identity(esp_ota_get_running_partition(), update_partition) == true) {
          ESP_LOGI(TAG, "The current running firmware is same as the firmware just downloaded");
          ESP_LOGE(TAG, "Firmware Upgrade Failed");
          publish_ota_data(OTA_FAILED);
          continue;
        }

        err = esp_ota_set_boot_partition(update_partition);
        if (err != ESP_OK) {
          ESP_LOGE(TAG, "esp_ota_set_boot_partition failed (%s)!", esp_err_to_name(err));
          http_cleanup(client);
          ESP_LOGE(TAG, "Firmware Upgrade Failed");
          publish_ota_data(OTA_FAILED);
          continue;
        }

        ESP_LOGI(TAG, "Firmware Upgrade Success, will restart in 10 seconds");
        publish_ota_data(OTA_SUCCESFULL);
        vTaskDelay(10000 / portTICK_PERIOD_MS);
        esp_restart();
      }
  }
}

#endif //defined(CONFIG_MQTT_OTA) && defined(CONFIG_TARGET_DEVICE_ESP32)
