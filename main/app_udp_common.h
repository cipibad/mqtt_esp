#ifndef UDP_COMMON__H
#define UDP_COMMON__H

typedef struct internal_msg {
  int qos;         // 16
  int retain;      // 16
  char topic[64];  // 64
  char data[64];   // 64
} internal_msg_t;

typedef struct ack_msg {
  int sequence;  // 16
} ack_msg_t;

typedef union msg {
  internal_msg_t i_msg;
  ack_msg_t a_msg;
} msg_t;
typedef struct udp_msg {
  short magic;       // 8
  short version;     // 4
  short msg_type;    // 4
  int sequence;      // 16
  msg_t msg;         // 160
  char padding[64];  // 64
} udp_msg_t;

#define MSG_TYPE_PUB 1
#define MSG_TYPE_PUB_ACK 2
#define MSG_TYPE_PING 3
#define MSG_TYPE_PING_ACK 4

#define INT_QUEUE_TIMEOUT (5 * 1000 / portTICK_PERIOD_MS)

#define QOS_0 0
#define QOS_1 1
#define NO_RETAIN 0
#define RETAIN 1
#define UDP_MAGIC ((short)0xABF3)

bool udp_publish_data(const char *topic, const char *data, int qos, int retain);

#ifdef CONFIG_NORTH_INTERFACE_UDP
#include "lwip/sockets.h"
#include "mdns.h"
int mdns_resolve_udp_iot_service(in_addr_t *a, in_port_t *p);
#endif  // CONFIG_NORTH_INTERFACE_UDP

void udp_server_task(void *pvParameters);
void udp_client_task(void *pvParameters);


#endif  // UDP_COMMON__H
