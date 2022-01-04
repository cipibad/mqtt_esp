#include "esp_system.h"
#ifdef CONFIG_MQTT_OPS
#include "esp_log.h"

#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"

#include <string.h>

#include "app_main.h"
#include "app_ops.h"

#include "app_publish_data.h"

static const char *TAG = "MQTTS_OPS";

void publish_ops_data()
{
  const char * topic = CONFIG_DEVICE_TYPE "/" CONFIG_CLIENT_ID "/evt/ops";
  char data[64];
  memset(data,0,64);

  sprintf(data, "{\"free_heap\":%d, \"min_free_heap\":%d}",
          esp_get_free_heap_size(),
          esp_get_minimum_free_heap_size()
          );

  publish_non_persistent_data(topic, data);
}


void ops_pub_task(void* pvParameters)
{
  ESP_LOGI(TAG, "Starting ops task");
  while (1) {
    ESP_LOGI(TAG, "Publishing ops data");
    publish_ops_data();
    vTaskDelay(60000 / portTICK_PERIOD_MS);
  }
}

#endif // CONFIG_MQTT_OPS
