#include "esp_system.h"
#if defined(CONFIG_NORTH_INTERFACE_UDP) || defined(CONFIG_SOUTH_INTERFACE_UDP)

#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

#include "esp_log.h"

#include "mbedtls/ctr_drbg.h"
#include "mbedtls/entropy.h"

#include "lwip/sockets.h"

#include "app_udp_common.h"

extern QueueHandle_t intMsgQueue;

#ifdef CONFIG_SOUTH_INTERFACE_UDP
extern in_addr_t client_addr;
extern bool client_connected;
#endif  // CONFIG_SOUTH_INTERFACE_UDP

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

void udp_client_task(void *pvParameters) {
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
#ifdef CONFIG_NORTH_INTERFACE_UDP
    in_addr_t addr = 0;
    in_port_t port = 0;
    while (mdns_resolve_udp_iot_service(&addr, &port) != 0) {
      vTaskDelay(3 * 1000 / portTICK_RATE_MS);
    }
    destAddr.sin_addr.s_addr = addr;
    destAddr.sin_port = htons(port);
#endif  // CONFIG_NORTH_INTERFACE_UDP

#ifdef CONFIG_SOUTH_INTERFACE_UDP
    destAddr.sin_port = htons(CONFIG_SOUTH_INTERFACE_UDP_PORT);
#endif  // CONFIG_SOUTH_INTERFACE_UDP

    destAddr.sin_family = AF_INET;
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
      ESP_LOGE(TAG, "random sequence init failed %d", ret);
    } else {
      ESP_LOGI(TAG, "random sequence init ok");
    }
    while (1) {
      if (xQueueReceive(intMsgQueue, &imsg, portMAX_DELAY)) {
#ifdef CONFIG_SOUTH_INTERFACE_UDP
        if (!client_connected) {
          ESP_LOGE(TAG, "Client not connected, canot message");
          continue;
        }

        destAddr.sin_addr.s_addr = client_addr;
#endif  // CONFIG_SOUTH_INTERFACE_UDP

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

#endif  // defined(CONFIG_NORTH_INTERFACE_UDP) ||
        // defined(CONFIG_SOUTH_INTERFACE_UDP)
