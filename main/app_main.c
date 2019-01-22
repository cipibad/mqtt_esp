/*******************************************************************************
 * Copyright (c) 2014 IBM Corp.
 *
 * All rights reserved. This program and the accompanying materials
 * are made available under the terms of the Eclipse Public License v1.0
 * and Eclipse Distribution License v1.0 which accompany this distribution.
 *
 * The Eclipse Public License is available at
 *    http://www.eclipse.org/legal/epl-v10.html
 * and the Eclipse Distribution License is available at
 *   http://www.eclipse.org/org/documents/edl-v10.php.
 *
 * Contributors:
 *    Ian Craggs - initial API and implementation and/or initial documentation
 *******************************************************************************/

#include <stddef.h>
#include <stdio.h>
#include <string.h>

#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event_loop.h"
#include "esp_log.h"
#include "nvs_flash.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"

#include "MQTTClient.h"


#include "app_relay.h"
#include "app_sensors.h"
#include "app_sensors_publish.h"
#include "app_ota.h"

int32_t wtemperature;
int16_t pressure;

/* The examples use simple WiFi configuration that you can set via
   'make menuconfig'.

   If you'd rather not, just change the below entries to strings with
   the config you want - ie #define EXAMPLE_WIFI_SSID "mywifissid"
*/
#define EXAMPLE_WIFI_SSID CONFIG_WIFI_SSID
#define EXAMPLE_WIFI_PASS CONFIG_WIFI_PASSWORD
#define FW_VERSION "0.02"

/* FreeRTOS event group to signal when we are connected & ready to make a request */
static EventGroupHandle_t wifi_event_group;
const int CONNECTED_BIT = BIT0;

EventGroupHandle_t mqtt_publish_event_group;
const int MQTT_PUBLISH_RELAYS_BIT = BIT0;
const int MQTT_PUBLISH_DHT22_BIT = BIT1;
const int NO_OTA_ONGOING_BIT = BIT2;

EventGroupHandle_t ota_event_group;
const int OTA_BIT = BIT0;



/* The event group allows multiple bits for each event,
   but we only care about one event - are we connected
   to the AP with an IP? */

#define MQTT_PORT    8883             /* MQTT Port*/

#define MQTT_CLIENT_THREAD_NAME         "mqtt_client_thread"
#define MQTT_CLIENT_THREAD_STACK_WORDS  8192
#define MQTT_CLIENT_THREAD_PRIO         8

static const char *TAG = "example";

#include "mqtt_iot_cipex_ro_cert.h"

ssl_ca_crt_key_t ssl_cck;

#define SSL_CA_CERT_KEY_INIT(s,a,b)  ((ssl_ca_crt_key_t *)s)->cacrt = a;\
                                     ((ssl_ca_crt_key_t *)s)->cacrt_len = b;\
                                     ((ssl_ca_crt_key_t *)s)->cert = NULL;\
                                     ((ssl_ca_crt_key_t *)s)->cert_len = 0;\
                                     ((ssl_ca_crt_key_t *)s)->key = NULL;\
                                     ((ssl_ca_crt_key_t *)s)->key_len = 0;

static esp_err_t event_handler(void *ctx, system_event_t *event)
{
    switch(event->event_id) {
    case SYSTEM_EVENT_STA_START:
        esp_wifi_connect();
        break;
    case SYSTEM_EVENT_STA_GOT_IP:
        xEventGroupSetBits(wifi_event_group, CONNECTED_BIT);
        break;
    case SYSTEM_EVENT_STA_DISCONNECTED:
        /* This is a workaround as ESP32 WiFi libs don't currently
           auto-reassociate. */
        esp_wifi_connect();
        xEventGroupClearBits(wifi_event_group, CONNECTED_BIT);
        break;
    default:
        break;
    }
    return ESP_OK;
}

static void initialise_wifi(void)
{
    tcpip_adapter_init();
    wifi_event_group = xEventGroupCreate();
    ESP_ERROR_CHECK( esp_event_loop_init(event_handler, NULL) );
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK( esp_wifi_init(&cfg) );
    ESP_ERROR_CHECK( esp_wifi_set_storage(WIFI_STORAGE_RAM) );
    wifi_config_t wifi_config = {
        .sta = {
            .ssid = EXAMPLE_WIFI_SSID,
            .password = EXAMPLE_WIFI_PASS,
        },
    };
    ESP_LOGI(TAG, "Setting WiFi configuration SSID %s...", wifi_config.sta.ssid);
    ESP_ERROR_CHECK( esp_wifi_set_mode(WIFI_MODE_STA) );
    ESP_ERROR_CHECK( esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config) );
    ESP_ERROR_CHECK( esp_wifi_start() );
}

static void otaCmdArrived(MessageData* data)
{
  printf("otaCmd arrived: %s\n", (char*)data->message->payload);
  xEventGroupClearBits(mqtt_publish_event_group, NO_OTA_ONGOING_BIT);
  xEventGroupSetBits(ota_event_group, OTA_BIT);
}

static void relay0CmdArrived(MessageData* data)
{
  printf("relayCmd arrived: %s\n", (char*)data->message->payload);
  handle_specific_relay_cmd(0, data->message);
}

static void relay1CmdArrived(MessageData* data)
{
  printf("relayCmd arrived: %s\n", (char*)data->message->payload);
  handle_specific_relay_cmd(1, data->message);
}

static void relay2CmdArrived(MessageData* data)
{
  printf("relayCmd arrived: %s\n", (char*)data->message->payload);
  handle_specific_relay_cmd(2, data->message);
}

static void relay3CmdArrived(MessageData* data)
{
  printf("relayCmd arrived: %s\n", (char*)data->message->payload);
  handle_specific_relay_cmd(3, data->message);
}

int publish_connected_data(MQTTClient* pClient)
{

  const char * connect_topic = CONFIG_MQTT_DEVICE_TYPE"/"CONFIG_MQTT_CLIENT_ID"/evt/connected";
  ESP_LOGI(TAG, "starting publish_connected_data");
  char data[256];
  memset(data,0,256);

  sprintf(data, "{\"v\":\""FW_VERSION"\"}");

  MQTTMessage message;
  message.qos = QOS2;
  message.retained = 1;
  message.payload = data;
  message.payloadlen = strlen(data);
  int rc;
  ESP_LOGI(TAG, "before MQTTPublish");
  if ((rc = MQTTPublish(pClient, connect_topic, &message)) != 0) {
    ESP_LOGI(TAG, "Return code from MQTT publish is %d\n", rc);
  } else {
    ESP_LOGI(TAG, "MQTT publish topic \"%s\"\n", connect_topic);
  }
  return rc;
}


static void mqtt_client_thread(void* pvParameters)
{

    MQTTClient client;
    Network network;
    int rc = 0, count = 0;
    MQTTPacket_connectData connectData = MQTTPacket_connectData_initializer;

    printf("mqtt client thread starts\n");

    /* Wait for the callback to set the CONNECTED_BIT in the
       event group.
    */
    xEventGroupWaitBits(wifi_event_group, CONNECTED_BIT,
                        false, true, portMAX_DELAY);
    ESP_LOGI(TAG, "Connected to AP");

    NetworkInitSSL(&network);
    MQTTClientInit(&client, &network, 0, NULL, 0, NULL, 0);

    SSL_CA_CERT_KEY_INIT(&ssl_cck, mqtt_iot_cipex_ro_pem, mqtt_iot_cipex_ro_pem_len);

    if ((rc = NetworkConnectSSL(&network, CONFIG_MQTT_SERVER, MQTT_PORT, &ssl_cck, TLSv1_1_client_method(), SSL_VERIFY_PEER, 8192)) != 0) {
       printf("Return code from network connect is %d\n", rc);
    }

    connectData.MQTTVersion = 3;
    connectData.clientID.cstring = CONFIG_MQTT_CLIENT_ID;
    connectData.username.cstring = CONFIG_MQTT_USERNAME;
    connectData.password.cstring = CONFIG_MQTT_PASSWORD;
    connectData.willFlag = 1;
    connectData.will.topicName.cstring = CONFIG_MQTT_DEVICE_TYPE"/"CONFIG_MQTT_CLIENT_ID"/evt/disconnected";
    connectData.keepAliveInterval = 60;

    while ((rc = MQTTConnect(&client, &connectData))) {
        printf("Return code from MQTT connect is %d\n", rc);
        vTaskDelay(5000 / portTICK_RATE_MS);  //send every 5 seconds
    }
    printf("MQTT Connected\n");
#define RELAY_TOPIC CONFIG_MQTT_DEVICE_TYPE"/"CONFIG_MQTT_CLIENT_ID"/cmd/relay"
#define OTA_TOPIC CONFIG_MQTT_DEVICE_TYPE"/"CONFIG_MQTT_CLIENT_ID"/cmd/ota"

#if defined(MQTT_TASK)

    if ((rc = MQTTStartTask(&client)) != pdPASS) {
        printf("Return code from start tasks is %d\n", rc);
    } else {
        printf("Use MQTTStartTask\n");
    }

#endif


    vTaskDelay(100 / portTICK_RATE_MS);

    if ((rc = MQTTSubscribe(&client, RELAY_TOPIC"/0", 2, relay0CmdArrived)) != 0) {
        printf("Return code from MQTT subscribe is %d\n", rc);
    } else {
        printf("MQTT subscribe to topic \"%s\"\n", RELAY_TOPIC"/0");
    }
    vTaskDelay(100 / portTICK_RATE_MS);

    if ((rc = MQTTSubscribe(&client, RELAY_TOPIC"/1", 2, relay1CmdArrived)) != 0) {
        printf("Return code from MQTT subscribe is %d\n", rc);
    } else {
        printf("MQTT subscribe to topic \"%s\"\n", RELAY_TOPIC"/1");
    }
    vTaskDelay(100 / portTICK_RATE_MS);

    if ((rc = MQTTSubscribe(&client, RELAY_TOPIC"/2", 2, relay2CmdArrived)) != 0) {
        printf("Return code from MQTT subscribe is %d\n", rc);
    } else {
        printf("MQTT subscribe to topic \"%s\"\n", RELAY_TOPIC"/2");
    }
    vTaskDelay(100 / portTICK_RATE_MS);

    if ((rc = MQTTSubscribe(&client, RELAY_TOPIC"/3", 2, relay3CmdArrived)) != 0) {
        printf("Return code from MQTT subscribe is %d\n", rc);
    } else {
        printf("MQTT subscribe to topic \"%s\"\n", RELAY_TOPIC"/3");
    }
    vTaskDelay(100 / portTICK_RATE_MS);

    if ((rc = MQTTSubscribe(&client, OTA_TOPIC, 2, otaCmdArrived)) != 0) {
        printf("Return code from MQTT subscribe is %d\n", rc);
    } else {
        printf("MQTT subscribe to topic \"%s\"\n", OTA_TOPIC);
    }

    vTaskDelay(100 / portTICK_RATE_MS);
    publish_connected_data(&client);
    vTaskDelay(100 / portTICK_RATE_MS);
    publish_relay_data(&client);

    while (++count) {
      xEventGroupWaitBits(mqtt_publish_event_group, MQTT_PUBLISH_RELAYS_BIT|MQTT_PUBLISH_DHT22_BIT , false, false, portMAX_DELAY);
      xEventGroupWaitBits(mqtt_publish_event_group, NO_OTA_ONGOING_BIT , false, false, portMAX_DELAY);
      if (xEventGroupGetBits(mqtt_publish_event_group) & MQTT_PUBLISH_RELAYS_BIT) {
        ESP_LOGI(TAG, "relay data to publish");
        xEventGroupClearBits(mqtt_publish_event_group, MQTT_PUBLISH_RELAYS_BIT);
        publish_relay_data(&client);
      }
      if (xEventGroupGetBits(mqtt_publish_event_group) & MQTT_PUBLISH_DHT22_BIT) {
        ESP_LOGI(TAG, "sensors data to publish");
        xEventGroupClearBits(mqtt_publish_event_group, MQTT_PUBLISH_DHT22_BIT);
        if (publish_sensors_data(&client) != 0) {
          ESP_LOGI(TAG, "failed su publish data, will reboot");
          vTaskDelay(5000 / portTICK_RATE_MS);
          exit(-1);
        };
      }
    }

    printf("mqtt_client_thread going to be deleted\n");
    vTaskDelete(NULL);
    return;
}

void app_main(void)
{
    esp_log_level_set("*", ESP_LOG_DEBUG);
    mqtt_publish_event_group = xEventGroupCreate();
    xEventGroupSetBits(mqtt_publish_event_group, NO_OTA_ONGOING_BIT);
    ota_event_group = xEventGroupCreate();
    ESP_ERROR_CHECK( nvs_flash_init() );
    initialise_wifi();
    relays_init();

    xTaskCreate(&mqtt_client_thread,
                MQTT_CLIENT_THREAD_NAME,
                MQTT_CLIENT_THREAD_STACK_WORDS,
                NULL,
                MQTT_CLIENT_THREAD_PRIO,
                NULL);
    xTaskCreate(sensors_read, "sensors_read", 2048, NULL, 9, NULL);
    xTaskCreate(handle_ota_task, "handle_ota_task", 2048, NULL, 10, NULL);

}
