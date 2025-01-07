#include "esp_system.h"
#ifdef CONFIG_NORTH_INTERFACE_UDP

#include <string.h>
#include <sys/param.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_wifi.h"
#include "esp_event_loop.h"
#include "esp_log.h"
#include "nvs_flash.h"

#include "lwip/err.h"
#include "lwip/sockets.h"
#include "lwip/sys.h"
#include <lwip/netdb.h>

#include "mbedtls/entropy.h"
#include "mbedtls/ctr_drbg.h"

#include "app_udp_common.h"

#define HOST_IP_ADDR "10.0.0.1"
#define PORT 12345


static const char *TAG = "udp_client";

extern QueueHandle_t intMsgQueue;


 void udp_client_task(void *pvParameters)
{
    ESP_LOGI(TAG, "udp_client_task thread created");

    char addr_str[128];
    int addr_family;
    int ip_protocol;
    struct sockaddr_in destAddr;

    internal_msg_t imsg;
    udp_msg_t umsg;
    umsg.magic = UDP_MAGIC;
    umsg.version = 0x1;

	mbedtls_entropy_context entropy;
	mbedtls_entropy_init( &entropy );

	mbedtls_ctr_drbg_context ctr_drbg;
	mbedtls_ctr_drbg_init( &ctr_drbg );

	// sets up the CTR_DRBG entropy source for future reseeds.
	const char * seed = "some random seed string";
	int ret = mbedtls_ctr_drbg_seed(&ctr_drbg, mbedtls_entropy_func, &entropy,
				(const unsigned char *)seed, strlen(seed));
	if (ret != 0) {
		ESP_LOGE(TAG, "mbedtls_ctr_drbg_seed failed %d", ret);
	} else {
		ESP_LOGI(TAG, "mbedtls_ctr_drbg_seed ok");
	}

    while (1) {
        destAddr.sin_addr.s_addr = inet_addr(HOST_IP_ADDR);
        destAddr.sin_family = AF_INET;
        destAddr.sin_port = htons(PORT);
        addr_family = AF_INET;
        ip_protocol = IPPROTO_IP;
        inet_ntoa_r(destAddr.sin_addr, addr_str, sizeof(addr_str) - 1);

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
        setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));
        int ret = mbedtls_ctr_drbg_random( &ctr_drbg, (unsigned char *) &umsg.sequence, sizeof(umsg.sequence));
        if (ret != 0) {
            ESP_LOGE(pcTaskGetName(NULL), "random sequence init failed %d", ret);
        } else {
            ESP_LOGI(pcTaskGetName(NULL), "random sequence init ok");
        }

        while (1) {
            if( xQueueReceive( intMsgQueue, &imsg, 2000 / portTICK_PERIOD_MS) )
              {
                umsg.sequence += 1;
                umsg.msg = imsg;
                int err = sendto(sock, &umsg, sizeof(umsg), 0, (struct sockaddr *)&destAddr, sizeof(destAddr));
                if (err < 0) {
                    ESP_LOGE(TAG, "Error occured during sending: errno %d", errno);
                } else {
                    ESP_LOGI(TAG, "Message sent");
                }
              } else {
                    ESP_LOGI(TAG, "No int message received");
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

#endif // CONFIG_NORTH_INTERFACE_UDP
