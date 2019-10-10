#ifndef APP_MQTT_H
#define APP_MQTT_H

/* some useful values for relay Json exchanges */
#define MAX_MQTT_DATA_LEN_RELAY 32
#define MAX_MQTT_DATA_THERMOSTAT 64
#define MAX_MQTT_DATA_SCHEDULER 64
#define JSON_BAD_RELAY_ID 255
#define JSON_BAD_RELAY_VALUE 255

#include "mqtt_client.h"

esp_mqtt_client_handle_t mqtt_init();
void mqtt_start(esp_mqtt_client_handle_t client);
void handle_mqtt_sub_pub(void* pvParameters);

/**
 * returns the value of a specific Json request
 * @param: tag - the json event, i.e: "state" or "onTimeout"
 * @param: event - mqtt event handler instance
 */
char get_relay_json_value(const char* tag, esp_mqtt_event_handle_t event);


char get_relay_id(esp_mqtt_event_handle_t event, const char * relayTopic);

bool handle_scheduler_mqtt_event(esp_mqtt_event_handle_t event);
bool handle_relay_cfg_mqtt_event(esp_mqtt_event_handle_t event);
bool handle_relay_cmd_mqtt_event(esp_mqtt_event_handle_t event);
bool handle_ota_mqtt_event(esp_mqtt_event_handle_t event);
bool handle_thermostat_mqtt_event(esp_mqtt_event_handle_t event);





#endif /* APP_MQTT_H */
