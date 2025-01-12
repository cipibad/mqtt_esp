#include "esp_system.h"
#ifdef CONFIG_SOUTH_INTERFACE_UDP

#include <lwip/netdb.h>
#include <string.h>
#include <sys/param.h>

#include "app_mqtt.h"
#include "app_udp_common.h"
#include "esp_event_loop.h"
#include "esp_log.h"
#include "esp_system.h"
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

/* The examples use simple WiFi configuration that you can set via
   'make menuconfig'.
   If you'd rather not, just change the below entries to strings with
   the config you want - ie #define EXAMPLE_WIFI_SSID "mywifissid"
*/

static const char *TAG = "udp_server";

void handle_udp_message(udp_msg_t *udp_msg) {
  if (udp_msg->magic != UDP_MAGIC) {
    ESP_LOGE(TAG, "Bad magic: 0x%04hX", udp_msg->magic);
    return;
  }

  if (udp_msg->msg_type == MSG_TYPE_PUB) {
    ESP_LOGI(TAG, "Got a PUB msg");
#ifdef CONFIG_NORTH_INTERFACE_MQTT
    mqtt_publish_data(udp_msg->msg.i_msg.topic, udp_msg->msg.i_msg.data,
                      udp_msg->msg.i_msg.qos, udp_msg->msg.i_msg.retain);
    // fixme send ack
    return;
#endif  // CONFIG_NORTH_INTERFACE_MQTT
    ESP_LOGE(TAG, "Incompatible north interface configured");
  }
}

void send_ack(int sock, udp_msg_t *rx_msg, struct sockaddr_in* addr, int tx_sequence) {
  udp_msg_t tx_msg;
  tx_msg.magic = UDP_MAGIC;
  tx_msg.version = 0x1;
  tx_msg.msg_type = MSG_TYPE_PUB_ACK;
  tx_msg.sequence = tx_sequence; tx_sequence += 1;
  tx_msg.msg.a_msg.sequence = rx_msg->sequence;
  int err = sendto(sock, &tx_msg, sizeof(tx_msg), 0,
                    (struct sockaddr *)addr, sizeof(*addr));
  if (err < 0) {
    ESP_LOGE(TAG, "Error occured during sending: errno %d", errno);
  } else {
    ESP_LOGI(TAG, "Message sent");
  }

}
void udp_north_server_task(void *pvParameters) {
  ESP_LOGI(TAG, "thread created");

  udp_msg_t rx_buffer;
  char addr_str[128];
  int addr_family;
  int ip_protocol;

  int tx_sequence;

  mbedtls_entropy_context entropy;
  mbedtls_entropy_init( &entropy );

  mbedtls_ctr_drbg_context ctr_drbg;
  mbedtls_ctr_drbg_init( &ctr_drbg );

  // sets up the CTR_DRBG entropy source for future reseeds.
  const char * seed = "some random seed string";
  int ret = mbedtls_ctr_drbg_seed(&ctr_drbg, mbedtls_entropy_func, &entropy,
  			(const unsigned char *)seed, strlen(seed));
  if (ret != 0) {
  	ESP_LOGE(pcTaskGetName(NULL), "mbedtls_ctr_drbg_seed failed %d", ret);
  } else {
  	ESP_LOGI(pcTaskGetName(NULL), "mbedtls_ctr_drbg_seed ok");
  }

  // unsigned char key[] = {0x13, 0x59, 0x14, 0x8A, 0xC2, 0x11, 0x6D, 0x6C,
  // 0x42, 0x06, 0xAE, 0x95, 0x4D, 0xE5, 0xFB, 0x00, 0x7A, 0x95, 0x15, 0xAA,
  // 0xCF, 0x0D, 0x32, 0x12, 0x2C, 0xEA, 0xD1, 0x85, 0x38, 0xA0, 0x33, 0xB3};
  // for (int i = 0; i < 32; i++)
  // {
  //     printf("%02X, ", key[i]);
  // }

  // // initializes the specified AES context.
  // mbedtls_aes_context aes_enc;
  // mbedtls_aes_init(&aes_enc);

  // // sets the encryption key.
  // mbedtls_aes_setkey_enc( &aes_enc, key, 256 );

  // unsigned char iv_enc[16];
  // unsigned char input [128];
  // unsigned char output[128];
  // for (int i=0;i<128;i++) input[i] = i;

  // // performs an AES-CFB128 encryption operation.
  // size_t iv_offset_enc = 0;
  // int crypt_len = 50;
  // ret = mbedtls_ctr_drbg_random( &ctr_drbg, iv_enc, 16);
  // if (ret != 0) {
  // 	ESP_LOGE(pcTaskGetName(NULL), "mbedtls_ctr_drbg_random failed %d", ret);
  // } else {
  // 	ESP_LOGI(pcTaskGetName(NULL), "mbedtls_ctr_drbg_random ok");
  // }
  // memset(output, 0x00, sizeof(output));
  // ret = mbedtls_aes_crypt_cfb128(&aes_enc, MBEDTLS_AES_ENCRYPT, crypt_len,
  // &iv_offset_enc, iv_enc, input, output); if (ret != 0) {
  // 	ESP_LOGE(pcTaskGetName(NULL),
  // "mbedtls_aes_crypt_cfb128(MBEDTLS_AES_ENCRYPT) failed %d", ret); } else {
  // 	ESP_LOGI(pcTaskGetName(NULL),
  // "mbedtls_aes_crypt_cfb128(MBEDTLS_AES_ENCRYPT) ok");
  // }
  // for (int i = 0; i < 128; i++)
  // {
  //     printf("%02X", input[i]);
  // }
  // printf("\n");
  // for (int i = 0; i < 128; i++)
  // {
  //     printf("%02X", output[i]);
  // }
  // printf("\n");

  // mbedtls_aes_context aes_dec;
  // mbedtls_aes_init(&aes_dec);

  // // sets the encryption key.
  // mbedtls_aes_setkey_enc( &aes_dec, key, 256 );

  // // performs an AES-CFB128 decryption operation.
  // unsigned char iv_dec[16];
  // size_t iv_offset_dec = 0;
  // memset(iv_dec, 0x00, sizeof(iv_dec));
  // memset(input, 0x00, sizeof(input));
  // ret = mbedtls_aes_crypt_cfb128(&aes_dec, MBEDTLS_AES_DECRYPT, crypt_len,
  // &iv_offset_dec, iv_dec, output, input); if (ret != 0) {
  // 	ESP_LOGE(pcTaskGetName(NULL),
  // "mbedtls_aes_crypt_cfb128(MBEDTLS_AES_DECRYPT) failed %d", ret); } else {
  // 	ESP_LOGI(pcTaskGetName(NULL),
  // "mbedtls_aes_crypt_cfb128(MBEDTLS_AES_DECRYPT) ok");
  // }
  // for (int i = 0; i < 128; i++)
  // {
  //     printf("%02X", input[i]);
  // }
  // printf("\n");

  // // releases and clears the specified AES context.
  // mbedtls_aes_free(&aes_enc);
  // mbedtls_aes_free(&aes_dec);

  while (1) {
    struct sockaddr_in destAddr;
    destAddr.sin_addr.s_addr = htonl(INADDR_ANY);
    destAddr.sin_family = AF_INET;
    destAddr.sin_port = htons(CONFIG_SOUTH_INTERFACE_UDP_PORT);
    addr_family = AF_INET;
    ip_protocol = IPPROTO_IP;
    inet_ntoa_r(destAddr.sin_addr, addr_str, sizeof(addr_str) - 1);

    ESP_LOGI(TAG, "Creating socket");

    int sock = socket(addr_family, SOCK_DGRAM, ip_protocol);
    if (sock < 0) {
      ESP_LOGE(TAG, "Unable to create socket: errno %d", errno);
      continue;
    }
    ESP_LOGI(TAG, "Socket created");

    int err = bind(sock, (struct sockaddr *)&destAddr, sizeof(destAddr));
    if (err < 0) {
      ESP_LOGE(TAG, "Socket unable to bind: errno %d", errno);
      continue;
    }
    ESP_LOGI(TAG, "Socket binded");

    int ret = mbedtls_ctr_drbg_random(
        &ctr_drbg, (unsigned char *)&tx_sequence, sizeof(tx_sequence));
    if (ret != 0) {
      ESP_LOGE(TAG, "random sequence init failed %d", ret);
    } else {
      ESP_LOGI(TAG, "random sequence init ok");
    }

    while (1) {
      ESP_LOGI(TAG, "Waiting for data");
      struct sockaddr_in sourceAddr;
      socklen_t socklen = sizeof(sourceAddr);
      int len = recvfrom(sock, &rx_buffer, sizeof(rx_buffer), 0,
                         (struct sockaddr *)&sourceAddr, &socklen);

      // Error occured during receiving
      if (len < 0) {
        ESP_LOGE(TAG, "recvfrom failed: errno %d", errno);
        break;
      }
      // Data received
      else {
        // Get the sender's ip address as string
        inet_ntoa_r(((struct sockaddr_in *)&sourceAddr)->sin_addr.s_addr,
                    addr_str, sizeof(addr_str) - 1);

        ESP_LOGI(TAG, "Received %d bytes from %s:", len, addr_str);
        handle_udp_message(&rx_buffer);
        send_ack(sock, &rx_buffer, &sourceAddr, tx_sequence);
      }
    }

    if (sock != -1) {
      ESP_LOGE(TAG, "Shutting down socket and restarting...");
      shutdown(sock, 0);
      close(sock);
    }
  }
    vTaskDelete(NULL);
}
#endif  // CONFIG_SOUTH_INTERFACE_UDP
