#ifndef APP_RELAY_H
#define APP_RELAY_H

#include "mqtt_client.h"

struct Relay
{
    unsigned char id;
    unsigned char status;
    unsigned char timeout;
    TimerHandle_t timer;
};


struct RelayCmdMessage
{
    unsigned char relayId;
    unsigned char relayValue;
};

struct RelayCfgMessage
{
    unsigned char relayId;
    unsigned char onTimeout;
};

void publish_all_relays_data(esp_mqtt_client_handle_t client);
void publish_relay_data(int id, esp_mqtt_client_handle_t client);

void publish_all_relays_cfg_data(esp_mqtt_client_handle_t client);


void relays_init(void);

void handle_relay_cmd_task(void* pvParameters);
void handle_relay_cfg_task(void* pvParameters);
void update_relay_state(int id, char value, esp_mqtt_client_handle_t client);

#endif /* APP_RELAY_H */
