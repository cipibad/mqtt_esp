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

#ifdef CONFIG_MQTT_OPS_HEAP
void publish_ops_heap_data()
{
  const char * topic = CONFIG_DEVICE_TYPE "/" CONFIG_CLIENT_ID "/evt/ops/heap";
  char data[64];
  memset(data,0,64);

  sprintf(data, "{\"free_heap\":%d, \"min_free_heap\":%d}",
          esp_get_free_heap_size(),
          esp_get_minimum_free_heap_size()
          );

  publish_non_persistent_data(topic, data);
}
#endif // CONFIG_MQTT_OPS_HEAP

#ifdef CONFIG_MQTT_OPS_STACK
void publish_ops_stack_data()
{
  const char * topic = CONFIG_DEVICE_TYPE "/" CONFIG_CLIENT_ID "/evt/ops/stack";
  const int DATA_SIZE = 256;
  int remaining_data;
  char data[DATA_SIZE];
  memset(data,0,64);
  strcpy(data, "{");
  remaining_data = DATA_SIZE - strlen(data) - 1;

  TaskStatus_t *pxTaskStatusArray;
  volatile UBaseType_t uxArraySize, x;
  uint32_t ulTotalRunTime;
  char task_data[32];

  /* Take a snapshot of the number of tasks in case it changes while this
  function is executing. */
  uxArraySize = uxTaskGetNumberOfTasks();
  ESP_LOGI(TAG, "Publishing tasks data for %lu tasks", uxArraySize);

  /* Allocate a TaskStatus_t structure for each task.  An array could be
  allocated statically at compile time. */
  pxTaskStatusArray = pvPortMalloc( uxArraySize * sizeof( TaskStatus_t ) );

  if( pxTaskStatusArray == NULL ) {
    ESP_LOGE(TAG, "Could not allocate tasks memory");
  } else {
    /* Generate raw status information about each task. */
    uxArraySize = uxTaskGetSystemState( pxTaskStatusArray,
                                        uxArraySize,
                                        &ulTotalRunTime );

  ESP_LOGI(TAG, "Got informations for %lu tasks, total runtime: %d",
          uxArraySize, ulTotalRunTime);

    /* For each populated position in the pxTaskStatusArray array,
    format the raw data as human readable ASCII data. */
    for( x = 0; x < uxArraySize; x++ )
    {
      sprintf( task_data, "\"%s\":%u,",
                pxTaskStatusArray[ x ].pcTaskName,
                pxTaskStatusArray[ x ].usStackHighWaterMark );
      if (strlen(task_data) < remaining_data) {
        strcat(data, task_data);
      } else {
        ESP_LOGE(TAG, "Not enough data space to read all tasks data, sending what we have");
        break;
      }
      remaining_data = DATA_SIZE - strlen(data) - 1;
    }

    /* The array is no longer needed, free the memory it consumes. */
    vPortFree( pxTaskStatusArray );
  }
  data[strlen(data) - 1] = 0;
  strcat(data, "}");

  publish_non_persistent_data(topic, data);
}
#endif // CONFIG_MQTT_OPS_STACK

void ops_pub_task(void* pvParameters)
{
  ESP_LOGI(TAG, "Starting ops task");
  while (1) {
    ESP_LOGI(TAG, "Publishing ops data");
    #ifdef CONFIG_MQTT_OPS_HEAP
    publish_ops_heap_data();
    #endif // CONFIG_MQTT_OPS_HEAP
    vTaskDelay(5 * 1000 / portTICK_PERIOD_MS);
    #ifdef CONFIG_MQTT_OPS_STACK
    publish_ops_stack_data();
    #endif // CONFIG_MQTT_OPS_STACK
    vTaskDelay(55 * 1000 / portTICK_PERIOD_MS);
  }
}

#endif // CONFIG_MQTT_OPS
