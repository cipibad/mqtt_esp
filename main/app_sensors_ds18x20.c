#ifdef CONFIG_DS18X20_SENSOR

#include "app_sensors_ds18x20.h"
#include "app_publish_data.h"
#include "app_logging.h"
#include <ds18x20.h>
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define MAX_SENSORS 8
static const gpio_num_t SENSOR_GPIO = CONFIG_DS18X20_SENSOR_GPIO;

static ds18x20_addr_t addrs[MAX_SENSORS];
static float temps[MAX_SENSORS];
static int sensor_count = 0;

void ds18x20_init(void)
{
}

void ds18x20_read(void)
{
  sensor_count = ds18x20_scan_devices(SENSOR_GPIO, addrs, MAX_SENSORS);
  if (sensor_count < 1) {
    LOGW(LOG_MODULE_DS18X20, "No sensors detected");
  } else {
    ds18x20_measure_and_read_multi(SENSOR_GPIO, addrs, sensor_count, temps);
    for (int j = 0; j < sensor_count; j++)
      {
        char addr[8+8+1];
        sprintf(addr, "%08x", (uint32_t)(addrs[j] >> 32));
        sprintf(addr + 8, "%08x", (uint32_t)addrs[j]);
        short temp_c = (short)(temps[j] * 10);
        LOGI(LOG_MODULE_DS18X20, "Sensor %s reports %d.%dC", addr, temp_c/10, abs(temp_c%10));
      }
  }
}

static void publish_ds18x20_temperature(int sensor_id)
{
  const char * temperature_topic = CONFIG_DEVICE_TYPE "/" CONFIG_CLIENT_ID "/evt/temperature";

  char topic[MAX_TOPIC_LEN];
  memset(topic,0,MAX_TOPIC_LEN);
  sprintf(topic, "%s/%08x%08x", temperature_topic,
          (uint32_t)(addrs[sensor_id] >> 32),
          (uint32_t)addrs[sensor_id]);

  publish_sensor_data(topic, temps[sensor_id] * 10);
}

void ds18x20_publish(void)
{
  for(int i=0; i<sensor_count; i++) {
    publish_ds18x20_temperature(i);
    vTaskDelay(50 / portTICK_PERIOD_MS);
  }
}

void ds18x20_publish_ha(int sensor_id)
{
  const char * topic = CONFIG_HOME_NAME "/" CONFIG_ROOM_NAME "/sensor";
  char payload[128];
  char addr_str[17];

  sprintf(addr_str, "%016llx", (unsigned long long)addrs[sensor_id]);

  sprintf(payload, "{\"source\":\"ds18x20\",\"address\":\"%s\",\"temperature\":%d.%02d}",
          addr_str,
          (int)(temps[sensor_id] * 10), abs((int)(temps[sensor_id] * 10) % 10));

  publish_non_persistent_data(topic, payload);
}

void ds18x20_publish_ha(void)
{
  for(int i=0; i<sensor_count; i++) {
    ds18x20_publish_ha(i);
    vTaskDelay(50 / portTICK_PERIOD_MS);
  }
}

int ds18x20_get_sensor_count(void)
{
  return sensor_count;
}

#endif // CONFIG_DS18X20_SENSOR
