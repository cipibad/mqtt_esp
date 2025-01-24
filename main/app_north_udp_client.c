#include "esp_system.h"
#ifdef CONFIG_NORTH_INTERFACE_UDP

#include <lwip/netdb.h>
#include <string.h>
#include <sys/param.h>

#include "app_udp_common.h"
#include "esp_event_loop.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/task.h"
#include "lwip/err.h"
#include "lwip/sockets.h"
#include "lwip/sys.h"
#include "mbedtls/ctr_drbg.h"
#include "mbedtls/entropy.h"
#include "mdns.h"
#include "nvs_flash.h"

static const char *if_str[] = {"STA", "AP", "ETH", "MAX"};
static const char *ip_protocol_str[] = {"V4", "V6", "MAX"};

extern QueueHandle_t intMsgQueue;

static const char *TAG = "udp_client";

void handle_ack_message(udp_msg_t *udp_msg, udp_msg_t *tx_msg) {
  if (udp_msg->magic != UDP_MAGIC) {
    ESP_LOGE(TAG, "Bad magic: 0x%04hX", udp_msg->magic);
    return;
  }
  if (udp_msg->msg_type == MSG_TYPE_PUB_ACK) {
    if (udp_msg->msg.a_msg.sequence == tx_msg->sequence) {
      ESP_LOGI(TAG, "ack for seq %d received", tx_msg->sequence);
      return;
    }
  }

  ESP_LOGE(TAG, "unhandled udp message");

  if (udp_msg->msg_type == MSG_TYPE_PUB) {
    // call mqtt handle stuff
  }

  if (udp_msg->msg_type == MSG_TYPE_PING) {
    // send ack
  }
  if (udp_msg->msg_type == MSG_TYPE_PING_ACK) {
    // check if we expect pink ack
  }
}

void udp_north_client_task(void *pvParameters) {
  ESP_LOGI(TAG, "udp_client_task thread created");

  int addr_family;
  int ip_protocol;
  struct sockaddr_in destAddr;

  internal_msg_t imsg;
  udp_msg_t umsg;
  umsg.magic = UDP_MAGIC;
  umsg.version = 0x1;

  mbedtls_entropy_context entropy;
  mbedtls_entropy_init(&entropy);

  mbedtls_ctr_drbg_context ctr_drbg;
  mbedtls_ctr_drbg_init(&ctr_drbg);

  // sets up the CTR_DRBG entropy source for future reseeds.
  const char *seed = "some random seed string";
  int ret = mbedtls_ctr_drbg_seed(&ctr_drbg, mbedtls_entropy_func, &entropy,
                                  (const unsigned char *)seed, strlen(seed));
  if (ret != 0) {
    ESP_LOGE(TAG, "mbedtls_ctr_drbg_seed failed %d", ret);
  } else {
    ESP_LOGI(TAG, "mbedtls_ctr_drbg_seed ok");
  }
  udp_msg_t rx_buffer;
  char addr_str[128];

  while (1) {
    in_addr_t addr = 0;
    in_port_t port = 0;
    while (addr == 0 || port == 0) {
      mdns_result_t *results = NULL;
      esp_err_t err = mdns_query_ptr("_uiot", "_udp", 3000, 20, &results);
      if (err) {
        ESP_LOGW(TAG, "Query Failed: %s, will retry!", esp_err_to_name(err));
        continue;
      }
      if (!results) {
        ESP_LOGW(TAG, "No results found, will retry!");
        continue;
      }

      mdns_result_t *r = results;
      mdns_ip_addr_t *a = NULL;
      int i = 1, t;
      while (r) {
        ESP_LOGI(TAG, "%d: Interface: %s, Type: %s\n", i++, if_str[r->tcpip_if],
                 ip_protocol_str[r->ip_protocol]);
        if (r->instance_name) {
          ESP_LOGI(TAG, "  PTR : %s\n", r->instance_name);
        }
        if (r->hostname) {
          ESP_LOGI(TAG, "  SRV : %s.local:%u\n", r->hostname, r->port);
          port = r->port;
        }
        if (r->txt_count) {
          ESP_LOGI(TAG, "  TXT : [%u] ", r->txt_count);
          for (t = 0; t < r->txt_count; t++) {
            printf("%s=%s; ", r->txt[t].key,
                   r->txt[t].value ? r->txt[t].value : "NULL");
          }
          ESP_LOGI(TAG, "\n");
        }
        a = r->addr;
        while (a) {
          if (a->addr.type == IPADDR_TYPE_V4) {
            ESP_LOGI(TAG, "  A   : " IPSTR "\n", IP2STR(&(a->addr.u_addr.ip4)));
            addr = a->addr.u_addr.ip4.addr;
          } else {
            ESP_LOGI(TAG, "  AAAA: " IPV6STR "\n",
                     IPV62STR(a->addr.u_addr.ip6));
          }
          a = a->next;
        }
        r = r->next;
      }
      mdns_query_results_free(results);
    }

    destAddr.sin_addr.s_addr = addr;
    destAddr.sin_family = AF_INET;
    destAddr.sin_port = htons(port);
    addr_family = AF_INET;
    ip_protocol = IPPROTO_IP;

    int sock = socket(addr_family, SOCK_DGRAM, ip_protocol);
    if (sock < 0) {
      ESP_LOGE(TAG, "Unable to create socket: errno %d", errno);
      break;
    }
    ESP_LOGI(TAG, "Socket created");
    struct timeval tv;
    tv.tv_sec = 1;
    tv.tv_usec = 0;
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    // setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));
    int ret = mbedtls_ctr_drbg_random(
        &ctr_drbg, (unsigned char *)&umsg.sequence, sizeof(umsg.sequence));
    if (ret != 0) {
      ESP_LOGE(pcTaskGetName(NULL), "random sequence init failed %d", ret);
    } else {
      ESP_LOGI(pcTaskGetName(NULL), "random sequence init ok");
    }
    while (1) {
      if (xQueueReceive(intMsgQueue, &imsg, portMAX_DELAY)) {
        umsg.msg_type = MSG_TYPE_PUB;
        umsg.sequence += 1;
        umsg.msg.i_msg = imsg;
        int err = sendto(sock, &umsg, sizeof(umsg), 0,
                         (struct sockaddr *)&destAddr, sizeof(destAddr));
        if (err < 0) {
          ESP_LOGE(TAG, "Error occured during sending: errno %d", errno);
          continue;
        } else {
          ESP_LOGI(TAG, "Message sent");
        }
      }
      struct sockaddr_in sourceAddr;
      socklen_t socklen = sizeof(sourceAddr);
      int len = recvfrom(sock, &rx_buffer, sizeof(rx_buffer), 0,
                         (struct sockaddr *)&sourceAddr, &socklen);
      // Error occured during receiving
      if (len < 0) {
        ESP_LOGW(TAG, "ACK recvfrom failed: errno %d", errno);
      }
      // Data received
      else {
        // Get the sender's ip address as string
        inet_ntoa_r(((struct sockaddr_in *)&sourceAddr)->sin_addr.s_addr,
                    addr_str, sizeof(addr_str) - 1);

        ESP_LOGI(TAG, "MSG Received %d bytes from %s:", len, addr_str);
        handle_ack_message(&rx_buffer, &umsg);
      }
    }

    if (sock != -1) {
      ESP_LOGE(TAG, "Shutting down socket and restarting...");
      shutdown(sock, 0);
      close(sock);
    }
  }
}

#endif  // CONFIG_NORTH_INTERFACE_UDP
