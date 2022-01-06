#include "esp_system.h"
#ifdef CONFIG_NORTH_INTERFACE_COAP

#include "app_coap_client.h"

#include <string.h>
#include <sys/socket.h>
#include <netdb.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"

#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_event_loop.h"

#include "nvs_flash.h"

#include "coap_config.h"
#include "coap.h"

#define COAP_DEFAULT_TIME_SEC 5
#define COAP_DEFAULT_TIME_USEC 0

extern EventGroupHandle_t wifi_event_group;
extern const int WIFI_CONNECTED_BIT;

const static char *TAG = "CoAP_client";

extern QueueHandle_t coapClientQueue;

void coap_publish_data(const char * topic,
                       const char * data)
{
    struct CoapMessage msg;
    strcpy(msg.resource, topic);
    strcpy(msg.data, data);

    if (xQueueSend(coapClientQueue
                    ,( void * )&msg
                    ,COAP_QUEUE_TIMEOUT) != pdPASS) {
        ESP_LOGE(TAG, "Cannot send to scheduleCfgQueue");
    }
}

static void message_handler(struct coap_context_t *ctx, const coap_endpoint_t *local_interface, const coap_address_t *remote,
              coap_pdu_t *sent, coap_pdu_t *received,
                const coap_tid_t id)
{
    unsigned char* data = NULL;
    size_t data_len;
    if (COAP_RESPONSE_CLASS(received->hdr->code) == 2) {
        if (coap_get_data(received, &data_len, &data)) {
            ESP_LOGI(TAG, "Received: %s", data);
        }
    }
}

void coap_client_thread(void *p)
{
    struct hostent *hp;
    struct ip4_addr *ip4_addr;

    coap_context_t*   ctx = NULL;
    coap_address_t    dst_addr, src_addr;
    static coap_uri_t uri;
    fd_set            readfds;
    struct timeval    tv;
    int flags, result;
    coap_pdu_t*       request = NULL;
    coap_tid_t tid = COAP_INVALID_TID;

    const int URI_HOSTNAME_SIZE = 32;
    char uri_hostname[URI_HOSTNAME_SIZE];

    const int BUFSIZE = 64;
    unsigned char _buf[BUFSIZE];
    unsigned char *buf;
    size_t buflen;

    char       server_uri[URI_HOSTNAME_SIZE + BUFSIZE];


    while (1) {
        //wait new message on  coapClientQueue
        const int timeout_in_seconds = 3 * 1000 / portTICK_PERIOD_MS;
        struct CoapMessage receivedData;
        xQueueReceive(coapClientQueue, &receivedData, timeout_in_seconds);

        // ESP_LOGI(TAG, "sending fake message");
        // strcpy(receivedData.resource, "sonoffb/ground59/evt/temperature/dht22");
        // strcpy(receivedData.data, "12345");

        strcpy(server_uri, "coap://" CONFIG_COAP_SERVER ":" CONFIG_COAP_PORT);
        strcat(server_uri, receivedData.resource);
        /* Wait for the callback to set the CONNECTED_BIT in the
           event group.
        */

        xEventGroupWaitBits(wifi_event_group, WIFI_CONNECTED_BIT,
                            false, true, portMAX_DELAY);
        ESP_LOGI(TAG, "AP is connected");

        if (coap_split_uri((const uint8_t *)server_uri, strlen(server_uri), &uri) == -1) {
            ESP_LOGE(TAG, "CoAP server uri error");
            continue;
        }

        if (uri.host.length >= URI_HOSTNAME_SIZE) {
            ESP_LOGE(TAG, "CoAP server uri host length to big");
            continue;
        }

        strncpy(uri_hostname, (const char *)uri.host.s, uri.host.length);
        uri_hostname[uri.host.length] = 0;

        hp = gethostbyname(uri_hostname);

        if (hp == NULL) {
            ESP_LOGE(TAG, "DNS lookup failed for %s", uri_hostname);
            vTaskDelay(1000 / portTICK_PERIOD_MS);
            continue;
        }

        /* Code to print the resolved IP.

           Note: inet_ntoa is non-reentrant, look at ipaddr_ntoa_r for "real" code */
        ip4_addr = (struct ip4_addr *)hp->h_addr;
        ESP_LOGI(TAG, "DNS lookup succeeded. IP=%s", inet_ntoa(*ip4_addr));

        coap_address_init(&src_addr);
        src_addr.addr.sin.sin_family      = AF_INET;
        src_addr.addr.sin.sin_port        = htons(0);
        src_addr.addr.sin.sin_addr.s_addr = INADDR_ANY;

        ctx = coap_new_context(&src_addr);
        if (ctx) {
            coap_address_init(&dst_addr);
            dst_addr.addr.sin.sin_family      = AF_INET;
            dst_addr.addr.sin.sin_port        = htons(COAP_DEFAULT_PORT);
            dst_addr.addr.sin.sin_addr.s_addr = ip4_addr->addr;

            request            = coap_new_pdu();
            if (request){
                request->hdr->type = COAP_MESSAGE_CON;
                request->hdr->id   = coap_new_message_id(ctx);
                request->hdr->code = COAP_REQUEST_PUT;

                if (uri.path.length)
                {
                    buflen = BUFSIZE;
                    buf = _buf;
                    int res = coap_split_path(uri.path.s, uri.path.length, buf, &buflen);
                    while (res--)
                    {
                        coap_add_option(request, COAP_OPTION_URI_PATH,
                                                 COAP_OPT_LENGTH(buf),
                                                 COAP_OPT_VALUE(buf));

                        buf += COAP_OPT_SIZE(buf);
                    }
                } else {
                    ESP_LOGI(TAG,"uri.path.length is not set");
                }
            
                coap_add_data(request, strlen(receivedData.data), (unsigned char *)receivedData.data);

                coap_show_pdu(request);

                coap_register_response_handler(ctx, message_handler);
                coap_send_confirmed(ctx, ctx->endpoint, &dst_addr, request);

                ESP_LOGI(TAG, "sending coap message");

                flags = fcntl(ctx->sockfd, F_GETFL, 0);
                fcntl(ctx->sockfd, F_SETFL, flags|O_NONBLOCK);

                tv.tv_usec = COAP_DEFAULT_TIME_USEC;
                tv.tv_sec = COAP_DEFAULT_TIME_SEC;

                for(;;) {
                    FD_ZERO(&readfds);
                    FD_CLR( ctx->sockfd, &readfds );
                    FD_SET( ctx->sockfd, &readfds );
                    result = select( ctx->sockfd+1, &readfds, 0, 0, &tv );
                    if (result > 0) {
                        if (FD_ISSET( ctx->sockfd, &readfds ))
                            coap_read(ctx);
                        ESP_LOGI(TAG, "received answer");
                        break;
                    } else if (result < 0) {
                        ESP_LOGE(TAG, "select error");
                        break;
                    } else {
                        ESP_LOGE(TAG, "select timeout");
                        break;
                    }
                }
            }
            coap_free_context(ctx);
        }
    }

    vTaskDelete(NULL);
}
#endif // CONFIG_NORTH_INTERFACE_COAP
