#include <string.h>
#include "esp_log.h"

#include "driver/gpio.h"
#include "rom/gpio.h"

#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/queue.h"

#include "app_main.h"
#include "app_sensors.h"
#include "app_mqtt.h"

extern QueueHandle_t sensorQueue;
extern QueueHandle_t mqttQueue;

#ifdef CONFIG_MQTT_SENSOR_DHT22
#include "dht.h"
int16_t dht22_temperature;
int16_t dht22_humidity;
#endif //CONFIG_MQTT_SENSOR_DHT22

#ifdef CONFIG_MQTT_SENSOR_DS18X20
#include <ds18x20.h>
#define MAX_SENSORS 8
static const gpio_num_t SENSOR_GPIO = CONFIG_MQTT_SENSOR_DS18X20_GPIO;
ds18x20_addr_t addrs[MAX_SENSORS];
float temps[MAX_SENSORS];
int32_t wtemperature;
int32_t ctemperature;
#endif // CONFIG_MQTT_SENSOR_DS18X20


#ifdef CONFIG_MQTT_SENSOR_BME280
#include "bme280.h"
#include "app_bme280.h"
int32_t bme280_pressure;
int32_t bme280_temperature;
int32_t bme280_humidity;
#endif //CONFIG_MQTT_SENSOR_BME280

#ifdef CONFIG_MQTT_THERMOSTAT
#include "app_thermostat.h"
extern QueueHandle_t thermostatQueue;
#endif // CONFIG_MQTT_THERMOSTAT

static const char *TAG = "app_sensors";

void read_sensors()
{
#ifdef CONFIG_MQTT_SENSOR_DHT22
  gpio_pad_select_gpio(CONFIG_MQTT_SENSOR_DHT22_GPIO);
  if (dht_read_data(DHT_TYPE_AM2301, CONFIG_MQTT_SENSOR_DHT22_GPIO, &dht22_humidity, &dht22_temperature) == ESP_OK)
    {
      ESP_LOGI(TAG, "Humidity: %d.%d%% Temp: %d.%dC",
               dht22_humidity/10, dht22_humidity%10 ,
               dht22_temperature/10, dht22_temperature%10);
    }
  else
    {
      ESP_LOGE(TAG, "Could not read data from DHT sensor");
    }
#endif //CONFIG_MQTT_SENSOR_DHT22

#ifdef CONFIG_MQTT_SENSOR_DS18X20
  wtemperature=0;
  ctemperature=0;

  int sensor_count = ds18x20_scan_devices(SENSOR_GPIO, addrs, MAX_SENSORS);
  if (sensor_count < 1) {
    ESP_LOGW(TAG, "No sensors detected!\n");
  } else {
    ds18x20_measure_and_read_multi(SENSOR_GPIO, addrs, sensor_count, temps);
    for (int j = 0; j < sensor_count; j++)
      {
        // The ds18x20 address is a 64-bit integer, but newlib-nano
        // printf does not support printing 64-bit values, so we
        // split it up into two 32-bit integers and print them
        // back-to-back to make it look like one big hex number.
        char addr[8+8+1];
        sprintf(addr, "%08x", (uint32_t)(addrs[j] >> 32));
        sprintf(addr + 8, "%08x", (uint32_t)addrs[j]);
        int temp_c = (int)(temps[j] * 10);
        ESP_LOGI(TAG,"Sensor %s reports %d.%dC", addr, temp_c/10, temp_c%10 );

#ifdef CONFIG_MQTT_THERMOSTAT
        if (strcmp(addr, CONFIG_MQTT_THERMOSTAT_DS18X20_SENSOR_ADDRESS) == 0) {
          wtemperature = temp_c;
        }
        else {
          ctemperature = temp_c;
        }
#else
        ctemperature = temp_c;
#endif // CONFIG_MQTT_THERMOSTAT
      }

  }
#endif // CONFIG_MQTT_SENSOR_DS18X20


#ifdef CONFIG_MQTT_SENSOR_BME280
  if (bme_read_data(&bme280_temperature, &bme280_pressure, &bme280_humidity) == ESP_OK)
    {
      ESP_LOGI(TAG, "Temp: %d.%02dC, Pressure: %d, Humidity: %d.%03d%%, ",  bme280_temperature/100,bme280_temperature%100, bme280_pressure, bme280_humidity/1000, bme280_humidity%1000);
    }
  else
    {
      ESP_LOGE(TAG, "Could not read data from BME sensor\n");
    }
#endif //CONFIG_MQTT_SENSOR_BME280

#ifdef CONFIG_MQTT_THERMOSTAT
  struct ThermostatSensorsMessage t = {wtemperature, ctemperature};
  struct ThermostatMessage tm;
  memset(&tm, 0, sizeof(struct ThermostatMessage));
  tm.data.sensorsData = t;
  if (xQueueSend( thermostatQueue
                  ,( void * )&tm
                  ,MQTT_QUEUE_TIMEOUT) != pdPASS) {
    ESP_LOGE(TAG, "Cannot send to thermostatQueue");
  }
#endif // CONFIG_MQTT_THERMOSTAT
  publish_sensors_data();
}

void vSensorTimerCallback( TimerHandle_t xTimer )
{
  ESP_LOGI(TAG, "sensor timer expiread, reading sensors");
  struct SensorMessage s;
  s.msgType = SENSOR_READ;
  if (xQueueSend( sensorQueue
                  ,( void * )&s
                  ,MQTT_QUEUE_TIMEOUT) != pdPASS) {
    ESP_LOGE(TAG, "Cannot send to sensorQueue");
  }
}

void sensors_task(void* pvParameters)
{
#ifdef CONFIG_MQTT_SENSOR_BME280
  //Don't forget to connect SDO to Vio too

	struct bme280_t bme280 = {
		.bus_write = BME280_I2C_bus_write,
		.bus_read = BME280_I2C_bus_read,
		.dev_addr = BME280_I2C_ADDRESS2,
		.delay_msec = BME280_delay_msek
	};
	esp_err_t err = BME280_I2C_init(&bme280,
                                  CONFIG_MQTT_SENSOR_BME280_SDA_GPIO,
                                  CONFIG_MQTT_SENSOR_BME280_SCL_GPIO);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "Cannot init bme280 sensor");
  }
#endif //CONFIG_MQTT_SENSOR_BME280


  //create period read timer
  TimerHandle_t th =
    xTimerCreate( "sensorTimer",           /* Text name. */
                  pdMS_TO_TICKS(60000),  /* Period. */
                  pdTRUE,                /* Autoreload. */
                  (void *)0,                  /* No ID. */
                  vSensorTimerCallback );  /* Callback function. */
  if( th != NULL ) {
    ESP_LOGI(TAG, "timer is created");
    if (xTimerStart(th, portMAX_DELAY) != pdFALSE){
      ESP_LOGI(TAG, "timer is active");
    }
  }

  while (1)
    {
      struct SensorMessage msg;
      while(1) {
        if(xQueueReceive(sensorQueue, &msg, portMAX_DELAY)) {
          if (msg.msgType == SENSOR_MQTT_CONNECTED) {
            publish_sensors_data();
          }
          if (msg.msgType == SENSOR_READ) {
            read_sensors();
          }
        }
      }

    }
}

void publish_sensors_data()
{
  struct MqttMsg m;
  memset(&m, 0, sizeof(struct MqttMsg));
  m.msgType = MQTT_PUBLISH;

  const char * topic = CONFIG_MQTT_DEVICE_TYPE "/" CONFIG_MQTT_CLIENT_ID "/evt/sensors";
  sprintf(m.publishData.topic, "%s", topic);

  char tstr[64];
  strcat(m.publishData.data, "{");

#ifdef CONFIG_MQTT_SENSOR_DHT22
  sprintf(tstr, "\"humidity\":%d.%d,\"temperature\":%d.%d,",
          dht22_humidity / 10, dht22_humidity % 10,
          dht22_temperature / 10, dht22_temperature % 10
          );
  strcat(m.publishData.data, tstr);
#endif //CONFIG_MQTT_SENSOR_DHT22

#ifdef CONFIG_MQTT_SENSOR_DS18X20
  sprintf(tstr, "\"wtemperature\":%d.%d,\"ctemperature\":%d.%d,",
          wtemperature / 10, wtemperature % 10,
          ctemperature / 10, ctemperature % 10);
  strcat(m.publishData.data, tstr);
#endif // CONFIG_MQTT_SENSOR_DS18X20

#ifdef CONFIG_MQTT_SENSOR_BME280
  ESP_LOGI(TAG, "Temp: %d.%02dC, Pressure: %d, Humidity: %d.%03d%%, ", bme280_temperature/100,bme280_temperature%100, bme280_pressure, bme280_humidity/1000, bme280_humidity%1000);

  sprintf(tstr, "\"humidity\":%d.%d,\"temperature\":%d.%d,\"pressure\":%d,",
          bme280_humidity/1000, bme280_humidity%1000,
          bme280_temperature/100,bme280_temperature%100,
          (int)(bme280_pressure*0.750061683)
          );
  strcat(m.publishData.data, tstr);
#endif //CONFIG_MQTT_SENSOR_BME280
  m.publishData.data[strlen(m.publishData.data)-1] = 0;
  strcat(m.publishData.data, "}");

  if (xQueueSend(mqttQueue
                 ,( void * )&m
                 ,MQTT_QUEUE_TIMEOUT) != pdPASS) {
    ESP_LOGE(TAG, "Cannot send sensorsData to mqttQueue");
  }
  ESP_LOGE(TAG, "Sent sensorsData to mqttQueue");
}

