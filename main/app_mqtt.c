#include "string.h"
#include "assert.h"

#include "esp_log.h"

#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/queue.h"

#include "app_esp8266.h"

#include "app_mqtt.h"


#include "app_relay.h"
#include "app_ota.h"
/* #include "app_sensors.h" */
/* #include "app_thermostat.h" */

#include "cJSON.h"



extern EventGroupHandle_t wifi_event_group;
extern EventGroupHandle_t mqtt_event_group;
extern const int CONNECTED_BIT;
extern const int SUBSCRIBED_BIT;
extern const int PUBLISHED_BIT;
extern const int INIT_FINISHED_BIT;


extern int16_t connect_reason;
extern const int mqtt_disconnect;
#define FW_VERSION "0.02.014"

extern QueueHandle_t relayQueue;
extern QueueHandle_t otaQueue;
extern QueueHandle_t thermostatQueue;
extern QueueHandle_t mqttQueue;


MQTTClient client;
Network network;
MQTTPacket_connectData connectData = MQTTPacket_connectData_initializer;

static const char *TAG = "MQTTS_MQTTS";

#ifdef MQTT_SSL

#include "mqtt_iot_cipex_ro_cert.h"

ssl_ca_crt_key_t ssl_cck;

#define SSL_CA_CERT_KEY_INIT(s,a,b)  ((ssl_ca_crt_key_t *)s)->cacrt = a;\
                                     ((ssl_ca_crt_key_t *)s)->cacrt_len = b;\
                                     ((ssl_ca_crt_key_t *)s)->cert = NULL;\
                                     ((ssl_ca_crt_key_t *)s)->cert_len = 0;\
                                     ((ssl_ca_crt_key_t *)s)->key = NULL;\
                                     ((ssl_ca_crt_key_t *)s)->key_len = 0;

#endif //MQTT_SSL


#define MAX_SUB 5 // must be lower that MAX_MESSAGE_HANDLERS
#define RELAY_TOPIC CONFIG_MQTT_DEVICE_TYPE "/" CONFIG_MQTT_CLIENT_ID "/cmd/relay"
#define OTA_TOPIC CONFIG_MQTT_DEVICE_TYPE "/" CONFIG_MQTT_CLIENT_ID "/cmd/ota"
#define THERMOSTAT_TOPIC CONFIG_MQTT_DEVICE_TYPE "/" CONFIG_MQTT_CLIENT_ID "/cmd/thermostat"

const char *SUBSCRIPTIONS[MAX_SUB] =
  {
    RELAY_TOPIC "/0",
    RELAY_TOPIC "/1",
    RELAY_TOPIC "/2",
    RELAY_TOPIC "/3",
    OTA_TOPIC/* , */
    /* THERMOSTAT_TOPIC */
  };

#define MIN(a,b) ((a<b)?a:b)

void dispatch_mqtt_event(MessageData* data)
{

  ESP_LOGI(TAG, "MQTT_EVENT_DATA");
  ESP_LOGI(TAG, "TOPIC=%.*s", data->topicName->lenstring.len, data->topicName->lenstring.data);
  ESP_LOGI(TAG, "DATA=%.*s", data->message->payloadlen, (char*) data->message->payload);

  const char * topic = data->topicName->lenstring.data;
  int topicLen = data->topicName->lenstring.len;
  if (strncmp(topic, RELAY_TOPIC, MIN(topicLen, strlen(RELAY_TOPIC))) == 0) {
    char id=255;
    if (strncmp(topic, RELAY_TOPIC "/0", MIN(topicLen, strlen(RELAY_TOPIC "/0"))) == 0) {
      id=0;
    }
    if (strncmp(topic, RELAY_TOPIC "/1", MIN(topicLen, strlen(RELAY_TOPIC "/1"))) == 0) {
      id=1;
    }
    if (strncmp(topic, RELAY_TOPIC "/2", MIN(topicLen, strlen(RELAY_TOPIC "/2"))) == 0) {
      id=2;
    }
    if (strncmp(topic, RELAY_TOPIC "/3", MIN(topicLen, strlen(RELAY_TOPIC "/3"))) == 0) {
      id=3;
    }
    if(id == 255)
      {
        ESP_LOGI(TAG, "unexpected relay id");
        return;
      }
    if (data->message->payloadlen >= 32 )
      {
        ESP_LOGI(TAG, "unexpected relay cmd payload");
        return;
      }
    char tmpBuf[32];
    memcpy(tmpBuf, data->message->payload, data->message->payloadlen);
    tmpBuf[data->message->payloadlen] = 0;
    cJSON * root   = cJSON_Parse(tmpBuf);
    if (root) {
      cJSON * state = cJSON_GetObjectItem(root,"state");
      if (state) {
        char value = state->valueint;
        ESP_LOGI(TAG, "id: %d, value: %d", id, value);
        struct RelayMessage r={id, value};
        ESP_LOGE(TAG, "Sending to relayQueue with timeout");
        if (xQueueSend( relayQueue
                        ,( void * )&r
                        ,MQTT_QUEUE_TIMEOUT) != pdPASS) {
          ESP_LOGE(TAG, "Cannot send to relayQueue");
        }
        ESP_LOGE(TAG, "Sending to relayQueue finished");
        return;
      }
    }
    ESP_LOGE(TAG, "bad json payload");
    return;
  }
  if (strncmp(topic, OTA_TOPIC, MIN(topicLen, strlen(OTA_TOPIC))) == 0) {
    struct OtaMessage o={"https://sw.iot.cipex.ro:8911/" CONFIG_MQTT_CLIENT_ID ".bin"};
    if (xQueueSend( otaQueue
                    ,( void * )&o
                    ,MQTT_QUEUE_TIMEOUT) != pdPASS) {
      ESP_LOGE(TAG, "Cannot send to relayQueue");

    }
  }

  /* if (strncmp(event->topic, THERMOSTAT_TOPIC, strlen(THERMOSTAT_TOPIC)) == 0) { */
  /*   if (event->data_len >= 64 ) */
  /*     { */
  /*       ESP_LOGI(TAG, "unextected relay cmd payload"); */
  /*       return; */
  /*     } */
  /*   char tmpBuf[64]; */
  /*   memcpy(tmpBuf, event->data, event->data_len); */
  /*   tmpBuf[event->data_len] = 0; */
  /*   cJSON * root   = cJSON_Parse(tmpBuf); */
  /*   if (root) { */
  /*     cJSON * ttObject = cJSON_GetObjectItem(root,"targetTemperature"); */
  /*     cJSON * ttsObject = cJSON_GetObjectItem(root,"targetTemperatureSensibility"); */
  /*     struct ThermostatMessage t = {0,0}; */
  /*     if (ttObject) { */
  /*       float targetTemperature = ttObject->valuedouble; */
  /*       ESP_LOGI(TAG, "targetTemperature: %f", targetTemperature); */
  /*       t.targetTemperature = targetTemperature; */
  /*     } */
  /*     if (ttsObject) { */
  /*       float targetTemperatureSensibility = ttsObject->valuedouble; */
  /*       ESP_LOGI(TAG, "targetTemperatureSensibility: %f", targetTemperatureSensibility); */
  /*       t.targetTemperatureSensibility = targetTemperatureSensibility; */
  /*     } */
  /*     if (t.targetTemperature || t.targetTemperatureSensibility) { */
  /*       if (xQueueSend( thermostatQueue */
  /*                       ,( void * )&t */
  /*                       ,MQTT_QUEUE_TIMEOUT) != pdPASS) { */
  /*         ESP_LOGE(TAG, "Cannot send to thermostatQueue"); */
  /*       } */
  /*     } */
  /*   } */
  /* } */
}

void publish_connected_data(MQTTClient* pclient)
{
  if (xEventGroupGetBits(mqtt_event_group) & INIT_FINISHED_BIT)
    {
      const char * connect_topic = CONFIG_MQTT_DEVICE_TYPE "/" CONFIG_MQTT_CLIENT_ID "/evt/connected";
      char data[256];
      memset(data,0,256);

      sprintf(data, "{\"v\":\"" FW_VERSION "\", \"r\":%d}", connect_reason);

      MQTTMessage message;
      message.qos = QOS1;
      message.retained = 1;
      message.payload = data;
      message.payloadlen = strlen(data);
      int rc = MQTTPublish(pclient, connect_topic, &message);
      if (rc == 0) {
        ESP_LOGI(TAG, "sent publish connected data successful, rc=%d", rc);
      } else {
        ESP_LOGI(TAG, "failed to publish connected data, rc=%d", rc);
      }
    }
}



static void mqtt_subscribe(MQTTClient* pclient)
{
  int rc;

  for (int i = 0; i < MAX_SUB; i++) {
    xEventGroupClearBits(mqtt_event_group, SUBSCRIBED_BIT);
    if ((rc = MQTTSubscribe(pclient, SUBSCRIPTIONS[i], QOS1, dispatch_mqtt_event)) == 0) {
      ESP_LOGI(TAG, "subscribed %s successful", SUBSCRIPTIONS[i]);
    } else {
      ESP_LOGI(TAG, "failed to subscribe %s, rc=%d", SUBSCRIPTIONS[i], rc);
    }
    //vTaskDelay(500 / portTICK_PERIOD_MS);

  }
  xEventGroupSetBits(mqtt_event_group, SUBSCRIBED_BIT);
      
}

MQTTClient* mqtt_init()
{
#ifdef MQTT_SSL
  SSL_CA_CERT_KEY_INIT(&ssl_cck, mqtt_iot_cipex_ro_pem, mqtt_iot_cipex_ro_pem_len);
  NetworkInitSSL(&network);
#else
  NetworkInit(&network);
#endif //MQTT_SSL
  if (MQTTClientInit(&client, &network, 0, NULL, 0, NULL, 0) == false) {
    ESP_LOGE(TAG, "mqtt init err");
  }

  connectData.MQTTVersion = 3;
  connectData.clientID.cstring = CONFIG_MQTT_CLIENT_ID;
  connectData.username.cstring = CONFIG_MQTT_USERNAME;
  connectData.password.cstring = CONFIG_MQTT_PASSWORD;
  connectData.willFlag = 1;
  connectData.will.topicName.cstring = CONFIG_MQTT_DEVICE_TYPE"/"CONFIG_MQTT_CLIENT_ID"/evt/disconnected";
  connectData.keepAliveInterval = MQTT_TIMEOUT;

  return &client;
}

void mqtt_connect(void *pvParameter){

  if (MAX_SUB > MAX_MESSAGE_HANDLERS) {
    ESP_LOGE(TAG, "MAX subscription limit(%d) passed", MAX_MESSAGE_HANDLERS);
  }
  
  MQTTClient* pclient = (MQTTClient*) pvParameter;
  int rc;
  while(true) {
    ESP_LOGI(TAG, "wait/check wifi connect...");
    xEventGroupWaitBits(wifi_event_group, CONNECTED_BIT, false, true, portMAX_DELAY);

#ifdef MQTT_SSL
    if ((rc = NetworkConnectSSL(&network, CONFIG_MQTT_SERVER, CONFIG_MQTT_PORT, &ssl_cck, TLSv1_1_client_method(), SSL_VERIFY_PEER, 8192)) != 0) {
#else
      if ((rc = NetworkConnect(&network, CONFIG_MQTT_SERVER, CONFIG_MQTT_PORT)) != 0) {
#endif //MQTT_SSL
        ESP_LOGE(TAG, "Return code from network connect is %d", rc);
        vTaskDelay(10000 / portTICK_PERIOD_MS);
        continue;
      }
      if ((rc = MQTTConnect(pclient, &connectData)) != 0) {
        ESP_LOGE(TAG, "Return code from MQTT connect is %d", rc);
        network.disconnect(&network);
        vTaskDelay(10000 / portTICK_PERIOD_MS);
        continue;
      }
      
      ESP_LOGI(TAG, "MQTT Connected");
      
#if defined(MQTT_TASK)
      
      if ((rc = MQTTStartTask(pclient)) != pdPASS) {
        ESP_LOGE(TAG, "Return code from start tasks is %d", rc);
      } else {
        ESP_LOGI(TAG, "Use MQTTStartTask");
      }

#endif
      xEventGroupSetBits(mqtt_event_group, CONNECTED_BIT);
      mqtt_subscribe(pclient);
      xEventGroupSetBits(mqtt_event_group, INIT_FINISHED_BIT);
      publish_connected_data(pclient);
      publish_all_relays_data(pclient);
      publish_ota_data(pclient, OTA_READY);
      /* publish_thermostat_data(pclient); */
      /* publish_sensor_data(pclient); */
      
      while (pclient->isconnected) {
        vTaskDelay(1000 / portTICK_PERIOD_MS);
      }
      xEventGroupClearBits(mqtt_event_group, CONNECTED_BIT | SUBSCRIBED_BIT | PUBLISHED_BIT | INIT_FINISHED_BIT);
      network.disconnect(&network);
      ESP_LOGE(TAG, "MQTT disconnected, will reconnect in 10 seconds");
      vTaskDelay(10000 / portTICK_PERIOD_MS);
  }
}

void mqtt_start (MQTTClient* pclient)
{
  ESP_LOGI(TAG, "init mqtt (re)connect thread, fw version " FW_VERSION);
  xTaskCreate(mqtt_connect, "mqtt_connect", configMINIMAL_STACK_SIZE * 3, pclient, 5, NULL);
  xEventGroupWaitBits(mqtt_event_group, CONNECTED_BIT, false, true, portMAX_DELAY);
}

