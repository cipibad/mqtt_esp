#include "esp_system.h"
#include "esp_log.h"
#include "nvs_flash.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"

#include "driver/gpio.h"

#include <string.h> //for memset
#include "MQTTClient.h"

#include "app_esp8266.h"

#include "app_wifi.h"
#include "app_mqtt.h"

/* #include "app_sensors.h" */
/* #include "app_thermostat.h" */
#include "app_relay.h"

#include "app_ota.h"

int32_t wtemperature;
int32_t ctemperature;
int16_t pressure;

#define FW_VERSION "0.02"


/* extern int targetTemperature; */
/* extern int targetTemperatureSensibility; */
/* extern const char * targetTemperatureTAG; */
/* extern const char * targetTemperatureSensibilityTAG; */

int16_t connect_reason;
const int boot = 0;
const int mqtt_disconnect = 1;

/* FreeRTOS event group to signal when we are connected & ready to make a request */
EventGroupHandle_t wifi_event_group;
EventGroupHandle_t mqtt_event_group;
const int CONNECTED_BIT = BIT0;
const int SUBSCRIBED_BIT = BIT1;
const int PUBLISHED_BIT = BIT2;
const int INIT_FINISHED_BIT = BIT3;

QueueHandle_t relayQueue;
QueueHandle_t otaQueue;
/* QueueHandle_t thermostatQueue; */
QueueHandle_t mqttQueue;

const int MQTT_PUBLISH_RELAYS_BIT = BIT5; //FIXME
const int MQTT_PUBLISH_DHT22_BIT = BIT6; //FIXME

static const char *TAG = "MQTT(S?)_MAIN";


#define BLINK_GPIO GPIO_NUM_13
void blink_task(void *pvParameter)
{
  /* Configure the IOMUX register for pad BLINK_GPIO (some pads are
     muxed to GPIO on reset already, but some default to other
     functions and need to be switched to GPIO. Consult the
     Technical Reference for a list of pads and their default
     functions.)
  */

  //gpio_pad_select_gpio(BLINK_GPIO); //FIXME why not implemented on esp8266??
  /* Set the GPIO as a push/pull output */
  gpio_set_direction(BLINK_GPIO, GPIO_MODE_OUTPUT);
  int interval;
  while(1) {
    gpio_set_level(BLINK_GPIO, ON);

    xEventGroupWaitBits(wifi_event_group, CONNECTED_BIT, false, true, portMAX_DELAY);
    interval=250;

    EventBits_t bits = xEventGroupGetBits(mqtt_event_group);
    if( ( bits & CONNECTED_BIT ) != 0 ) {
      interval=500;
    }

    vTaskDelay(interval / portTICK_PERIOD_MS);
    gpio_set_level(BLINK_GPIO, OFF);
    vTaskDelay(interval / portTICK_PERIOD_MS);
  }
}


/* static void relay0CmdArrived(MessageData* data) */
/* { */
/*   handle_specific_relay_cmd(0, data->message); */
/* } */

/* static void relay1CmdArrived(MessageData* data) */
/* { */
/*   handle_specific_relay_cmd(1, data->message); */
/* } */

/* static void relay2CmdArrived(MessageData* data) */
/* { */
/*   handle_specific_relay_cmd(2, data->message); */
/* } */

/* static void relay3CmdArrived(MessageData* data) */
/* { */
/*   handle_specific_relay_cmd(3, data->message); */
/* } */

/* int publish_connected_data(MQTTClient* pClient) */
/* { */

/*   const char * connect_topic = CONFIG_MQTT_DEVICE_TYPE"/"CONFIG_MQTT_CLIENT_ID"/evt/connected"; */
/*   char data[256]; */
/*   memset(data,0,256); */

/*   sprintf(data, "{\"v\":\""FW_VERSION"\"}"); */

/*   MQTTMessage message; */
/*   message.qos = QOS0; */
/*   message.retained = 1; */
/*   message.payload = data; */
/*   message.payloadlen = strlen(data); */
/*   int rc; */
/*   if ((rc = MQTTPublish(pClient, connect_topic, &message)) != 0) { */
/*     ESP_LOGI(TAG, "Return code from MQTT publish is %d", rc); */
/*   } else { */
/*     ESP_LOGI(TAG, "MQTT published topic \"%s\"", connect_topic); */
/*   } */
/*   return rc; */
/* } */


/* static void mqtt_client_thread(void *pvParameters) */
/* { */
/*     char *payload = NULL; */
/*     MQTTClient client; */
/*     Network network; */
/*     int rc = 0; */
/*     MQTTPacket_connectData connectData = MQTTPacket_connectData_initializer; */

/*     //#define MQTT_SSL 0 */

/* #ifdef MQTT_SSL */
/*     SSL_CA_CERT_KEY_INIT(&ssl_cck, mqtt_iot_cipex_ro_pem, mqtt_iot_cipex_ro_pem_len); */
/*     NetworkInitSSL(&network); */
/* #else */
/*     NetworkInit(&network); */
/* #endif //MQTT_SSL */

/*     if (MQTTClientInit(&client, &network, 0, NULL, 0, NULL, 0) == false) { */
/*         ESP_LOGE(TAG, "mqtt init err"); */
/*     } */

/*     payload = malloc(CONFIG_MQTT_PAYLOAD_BUFFER); */

/*     if (!payload) { */
/*         ESP_LOGE(TAG, "mqtt malloc err"); */
/*     } else { */
/*         memset(payload, 0x0, CONFIG_MQTT_PAYLOAD_BUFFER); */
/*     } */

/*     for (;;) { */
/*         ESP_LOGI(TAG, "wait wifi connect..."); */
/*         xEventGroupWaitBits(wifi_event_group, CONNECTED_BIT, false, true, portMAX_DELAY); */

/* #ifdef MQTT_SSL */
/*     if ((rc = NetworkConnectSSL(&network, CONFIG_MQTT_SERVER, CONFIG_MQTT_PORT, &ssl_cck, TLSv1_1_client_method(), SSL_VERIFY_PEER, 8192)) != 0) { */
/* #else */
/*     if ((rc = NetworkConnect(&network, CONFIG_MQTT_SERVER, CONFIG_MQTT_PORT)) != 0) { */
/* #endif //MQTT_SSL */
/*         ESP_LOGE(TAG, "Return code from network connect is %d", rc); */
/*         continue; */
/*     } */

/*     connectData.MQTTVersion = 3; */
/*     connectData.clientID.cstring = CONFIG_MQTT_CLIENT_ID; */
/*     connectData.username.cstring = CONFIG_MQTT_USERNAME; */
/*     connectData.password.cstring = CONFIG_MQTT_PASSWORD; */
/*     connectData.willFlag = 1; */
/*     connectData.will.topicName.cstring = CONFIG_MQTT_DEVICE_TYPE"/"CONFIG_MQTT_CLIENT_ID"/evt/disconnected"; */
/*     connectData.keepAliveInterval = 60; */
/*         ESP_LOGI(TAG, "MQTT Connecting"); */

/*         if ((rc = MQTTConnect(&client, &connectData)) != 0) { */
/*             ESP_LOGE(TAG, "Return code from MQTT connect is %d", rc); */
/*             network.disconnect(&network); */
/*             continue; */
/*         } */

/*         ESP_LOGI(TAG, "MQTT Connected"); */

/* #if defined(MQTT_TASK) */

/*         if ((rc = MQTTStartTask(&client)) != pdPASS) { */
/*             ESP_LOGE(TAG, "Return code from start tasks is %d", rc); */
/*         } else { */
/*             ESP_LOGI(TAG, "Use MQTTStartTask"); */
/*         } */

/* #endif */

/*         /\* if ((rc = MQTTSubscribe(&client, RELAY_TOPIC"/0", 1, relay0CmdArrived)) != 0) { *\/ */
/*         /\*     ESP_LOGE(TAG, "Return code from MQTT subscribe is %d", rc); *\/ */
/*         /\*     network.disconnect(&network); *\/ */
/*         /\*     continue; *\/ */
/*         /\* } *\/ */
/*         /\* ESP_LOGI(TAG, "MQTT subscribe to topic %s OK", RELAY_TOPIC"/0"); *\/ */

/*         /\* if ((rc = MQTTSubscribe(&client, RELAY_TOPIC"/1", 1, relay1CmdArrived)) != 0) { *\/ */
/*         /\*     ESP_LOGE(TAG, "Return code from MQTT subscribe is %d", rc); *\/ */
/*         /\*     network.disconnect(&network); *\/ */
/*         /\*     continue; *\/ */
/*         /\* } *\/ */
/*         /\* ESP_LOGI(TAG, "MQTT subscribe to topic %s OK", RELAY_TOPIC"/1"); *\/ */

/*         /\* if ((rc = MQTTSubscribe(&client, RELAY_TOPIC"/2", 1, relay2CmdArrived)) != 0) { *\/ */
/*         /\*     ESP_LOGE(TAG, "Return code from MQTT subscribe is %d", rc); *\/ */
/*         /\*     network.disconnect(&network); *\/ */
/*         /\*     continue; *\/ */
/*         /\* } *\/ */
/*         /\* ESP_LOGI(TAG, "MQTT subscribe to topic %s OK", RELAY_TOPIC"/2"); *\/ */

/*         /\* if ((rc = MQTTSubscribe(&client, RELAY_TOPIC"/3", 1, relay3CmdArrived)) != 0) { *\/ */
/*         /\*     ESP_LOGE(TAG, "Return code from MQTT subscribe is %d", rc); *\/ */
/*         /\*     network.disconnect(&network); *\/ */
/*         /\*     continue; *\/ */
/*         /\* } *\/ */
/*         /\* ESP_LOGI(TAG, "MQTT subscribe to topic %s OK", RELAY_TOPIC"/3"); *\/ */

/*         if ((rc = MQTTSubscribe(&client, OTA_TOPIC, 1, otaCmdArrived)) != 0) { */
/*             ESP_LOGE(TAG, "Return code from MQTT subscribe is %d", rc); */
/*             network.disconnect(&network); */
/*             continue; */
/*         } */

/*         ESP_LOGI(TAG, "MQTT subscribe to topic %s OK", OTA_TOPIC); */

/*         publish_connected_data(&client); */
/*         publish_relay_data(&client); */


/*         for (;;) { */
/*           xEventGroupWaitBits(mqtt_event_group, MQTT_PUBLISH_RELAYS_BIT|MQTT_PUBLISH_DHT22_BIT , false, false, portMAX_DELAY); */
/*           xEventGroupWaitBits(mqtt_event_group, NO_OTA_ONGOING_BIT , false, false, portMAX_DELAY); */
/*           if (xEventGroupGetBits(mqtt_event_group) & MQTT_PUBLISH_RELAYS_BIT) { */
/*             xEventGroupClearBits(mqtt_event_group, MQTT_PUBLISH_RELAYS_BIT); */
/*             publish_relay_data(&client); */
/*           } */
/*           if (xEventGroupGetBits(mqtt_event_group) & MQTT_PUBLISH_DHT22_BIT) { */
/*             xEventGroupClearBits(mqtt_event_group, MQTT_PUBLISH_DHT22_BIT); */
/*             if (publish_sensors_data(&client) != 0) { */
/*               ESP_LOGI(TAG, "failed to publish data, will reconnect"); */
/*               break; */
/*             } */
/*           } */
/*         } */
/*         network.disconnect(&network); */
/*     } */

/*     ESP_LOGW(TAG, "mqtt_client_thread going to be deleted"); */
/*     vTaskDelete(NULL); */
/*     return; */
/* } */

void app_main(void)
{
  connect_reason=boot;
  ESP_LOGI(TAG, "[APP] Startup..");
  ESP_LOGI(TAG, "[APP] Free memory: %d bytes", esp_get_free_heap_size());
  ESP_LOGI(TAG, "[APP] IDF version: %s", esp_get_idf_version());

  esp_log_level_set("*", ESP_LOG_INFO);
  esp_log_level_set("MQTT_CLIENT", ESP_LOG_VERBOSE);
  esp_log_level_set("TRANSPORT_TCP", ESP_LOG_VERBOSE);
  esp_log_level_set("TRANSPORT_SSL", ESP_LOG_VERBOSE);
  esp_log_level_set("TRANSPORT", ESP_LOG_VERBOSE);
  esp_log_level_set("OUTBOX", ESP_LOG_VERBOSE);

  relays_init();

  mqtt_event_group = xEventGroupCreate();
  wifi_event_group = xEventGroupCreate();

  /* thermostatQueue = xQueueCreate(1, sizeof(struct ThermostatMessage) ); */
  relayQueue = xQueueCreate(1, sizeof(struct RelayMessage) );
  otaQueue = xQueueCreate(1, sizeof(struct OtaMessage) );
  mqttQueue = xQueueCreate(1, sizeof(void *) );

  xTaskCreate(blink_task, "blink_task", configMINIMAL_STACK_SIZE, NULL, 3, NULL);

  esp_err_t err = nvs_flash_init();
  if (err == ESP_ERR_NVS_NO_FREE_PAGES) {
    // NVS partition was truncated and needs to be erased
    // Retry nvs_flash_init
    ESP_ERROR_CHECK(nvs_flash_erase());
    err = nvs_flash_init();
  }
  ESP_ERROR_CHECK( err );

  ESP_LOGI(TAG, "nvs_flash_init done");

  /* err=read_thermostat_nvs(targetTemperatureTAG, &targetTemperature); */
  /* ESP_ERROR_CHECK( err ); */

  /* err=read_thermostat_nvs(targetTemperatureSensibilityTAG, &targetTemperatureSensibility); */
  /* ESP_ERROR_CHECK( err ); */

  MQTTClient* client = mqtt_init();

  /* xTaskCreate(sensors_read, "sensors_read", configMINIMAL_STACK_SIZE * 3, (void *)client, 10, NULL); */

  xTaskCreate(handle_relay_cmd_task, "handle_relay_cmd_task", configMINIMAL_STACK_SIZE * 3, (void *)client, 5, NULL);
  xTaskCreate(handle_ota_update_task, "handle_ota_update_task", configMINIMAL_STACK_SIZE * 7, (void *)client, 5, NULL);
  /* xTaskCreate(handle_thermostat_cmd_task, "handle_thermostat_cmd_task", configMINIMAL_STACK_SIZE * 3, (void *)client, 5, NULL); */

  wifi_init();

  mqtt_start(client);

}
