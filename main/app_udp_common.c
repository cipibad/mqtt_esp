#include "esp_system.h"
#if defined(CONFIG_NORTH_INTERFACE_UDP) || defined(CONFIG_SOUTH_INTERFACE_UDP)

#include <string.h>

#include "app_udp_common.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

extern QueueHandle_t intMsgQueue;
static const char *TAG = "udp_common";

bool udp_publish_data(const char *topic, const char *data, int qos,
                      int retain) {
  internal_msg_t imsg;
  if (strlen(topic) < sizeof(imsg.topic)) {
    strcpy(imsg.topic, topic);
  } else {
    ESP_LOGE(TAG, "topic to long to send on int msg");
  }

  if (strlen(data) < sizeof(imsg.data)) {
    strcpy(imsg.data, data);
  } else {
    ESP_LOGE(TAG, "data to long to send on int msg");
  }
  imsg.qos = qos;
  imsg.retain = retain;
  if (xQueueSend(intMsgQueue, (void *)&imsg, INT_QUEUE_TIMEOUT) != pdPASS) {
    ESP_LOGE(TAG, "Cannot send to intMsgQueue");
    return false;
  }
  return true;
}
#endif  // defined(CONFIG_NORTH_INTERFACE_UDP) ||
        // defined(CONFIG_SOUTH_INTERFACE_UDP)

#ifdef CONFIG_NORTH_INTERFACE_UDP
int mdns_resolve_udp_iot_service(in_addr_t *p_addr, in_port_t *p_port) {
  static const char *if_str[] = {"STA", "AP", "ETH", "MAX"};
  static const char *ip_protocol_str[] = {"V4", "V6", "MAX"};

  in_addr_t addr = 0;
  in_port_t port = 0;
  mdns_result_t *results = NULL;
  esp_err_t err = mdns_query_ptr("_uiot", "_udp", 3000, 20, &results);
  if (err) {
    ESP_LOGW(TAG, "Query Failed: %s", esp_err_to_name(err));
    return -1;
  }
  if (!results) {
    ESP_LOGW(TAG, "No results found");
    return -2;
  }

  mdns_result_t *r = results;
  mdns_ip_addr_t *a = NULL;
  int i = 1, t;
  while (r) {
    ESP_LOGD(TAG, "%d: Interface: %s, Type: %s\n", i++, if_str[r->tcpip_if],
             ip_protocol_str[r->ip_protocol]);
    if (r->instance_name) {
      ESP_LOGD(TAG, "  PTR : %s\n", r->instance_name);
    }
    if (r->hostname) {
      ESP_LOGD(TAG, "  SRV : %s.local:%u\n", r->hostname, r->port);
      port = r->port;
    }
    if (r->txt_count) {
      ESP_LOGD(TAG, "  TXT : [%u] ", r->txt_count);
      for (t = 0; t < r->txt_count; t++) {
        ESP_LOGD(TAG, "%s=%s; ", r->txt[t].key,
                 r->txt[t].value ? r->txt[t].value : "NULL");
      }
    }
    a = r->addr;
    while (a) {
      if (a->addr.type == IPADDR_TYPE_V4) {
        ESP_LOGI(TAG, "  A   : " IPSTR "\n", IP2STR(&(a->addr.u_addr.ip4)));
        addr = a->addr.u_addr.ip4.addr;
      } else {
        ESP_LOGI(TAG, "  AAAA: " IPV6STR "\n", IPV62STR(a->addr.u_addr.ip6));
      }
      a = a->next;
    }
    r = r->next;
  }
  mdns_query_results_free(results);
  *p_addr = addr;
  *p_port = port;
  return 0;
}
#endif  // CONFIG_NORTH_INTERFACE_UDP
