#ifndef APP_RELAY_H
#define APP_RELAY_H

struct RelayMessage
{
    char relayId;
    char relayValue;
};

#ifdef ESP8266
#include "MQTTClient.h"
void publish_all_relays_data(MQTTClient* pClient);
void publish_relay_data(int id, MQTTClient* pClient);
#endif //ESP8266

#ifdef ESP32
#include "mqtt_client.h"
void publish_relay_data(esp_mqtt_client_handle_t client);
#endif //ESP32

void relays_init(void);
void handle_relay_cmd_task(void* pvParameters);
void update_relay_state(int id, char value, MQTTClient* client);

#endif /* APP_RELAY_H */
