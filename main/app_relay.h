#ifndef APP_RELAY_H
#define APP_RELAY_H

enum RelayMsgType {
  RELAY_MQTT_CONNECTED = 1,
  RELAY_CMD = 2
};


struct RelayCmdMessage
{
  unsigned char relayId;
  unsigned char relayValue;
};

struct RelayMessage {
  enum RelayMsgType msgType;
  struct RelayCmdMessage relayData;
};


enum RelayCfgMsgType {
  RELAY_CFG_MQTT_CONNECTED = 1,
  RELAY_CFG = 2
};

struct RelayCfgMessage
{
    unsigned char relayId;
    unsigned char onTimeout;
};

struct RelayCMessage {
  enum RelayCfgMsgType msgType;
  struct RelayCfgMessage relayCfg;
};

void publish_relay_data(int id);

void relays_init(void);

void handle_relay_cmd_task(void* pvParameters);
void handle_relay_cfg_task(void* pvParameters);
void update_relay_state(int id, char value);

#endif /* APP_RELAY_H */
