#ifndef APP_RELAY_H
#define APP_RELAY_H


#define RELAY_CMD_STATUS 1
#define RELAY_CMD_SLEEP  2

#define RELAY_STATUS_OFF   0
#define RELAY_STATUS_ON    1

struct RelayMessage {
  unsigned char msgType;
  unsigned char relayId;
  int data;
};

void publish_all_relays_status();

void publish_all_relays_timeout();


void relays_init(void);

void handle_relay_task(void* pvParameters);
void update_relay_status(int id, char value);

#define RELAY_TIMEOUT 30
#define RELAY_QUEUE_TIMEOUT (RELAY_TIMEOUT * 1000 / portTICK_PERIOD_MS)

#endif /* APP_RELAY_H */
