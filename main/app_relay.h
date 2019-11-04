#ifndef APP_RELAY_H
#define APP_RELAY_H

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

void publish_all_relays_data();
void publish_relay_data(int id);

void publish_all_relays_cfg_data();


void relays_init(void);

void handle_relay_cmd_task(void* pvParameters);
void handle_relay_cfg_task(void* pvParameters);
void update_relay_state(int id, char value);

#define RELAY_TIMEOUT 30
#define RELAY_QUEUE_TIMEOUT (RELAY_TIMEOUT * 1000 / portTICK_PERIOD_MS)

#endif /* APP_RELAY_H */
