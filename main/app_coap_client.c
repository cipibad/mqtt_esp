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

#include "coap.h"

/* The examples use simple WiFi configuration that you can set via
   'make menuconfig'.

   If you'd rather not, just change the below entries to strings with
   the config you want - ie #define EXAMPLE_WIFI_SSID "mywifissid"
*/
#define EXAMPLE_WIFI_SSID CONFIG_WIFI_SSID
#define EXAMPLE_WIFI_PASS CONFIG_WIFI_PASSWORD

#define COAP_DEFAULT_TIME_SEC 5
#define COAP_DEFAULT_TIME_USEC 0

/* The examples use uri "coap://californium.eclipse.org" that
   you can set via 'make menuconfig'.

   If you'd rather not, just change the below entries to strings with
   the config you want - ie #define COAP_DEFAULT_DEMO_URI "coap://californium.eclipse.org"
*/
#define COAP_DEFAULT_DEMO_URI "coap://10.0.0.1/sonoffb/ground59/evt/temperature/dht22"

extern EventGroupHandle_t wifi_event_group;
extern const int WIFI_CONNECTED_BIT;

void coap_publish_data(const char * topic,
                       const char * data)
// FIXME
{}

/* The event group allows multiple bits for each event,
   but we only care about one event - are we connected
   to the AP with an IP? */
const static int CONNECTED_BIT = BIT0;

const static char *TAG = "CoAP_client";

static void message_handler(struct coap_context_t *ctx, const coap_endpoint_t *local_interface, const coap_address_t *remote,
              coap_pdu_t *sent, coap_pdu_t *received,
                const coap_tid_t id)
{
    unsigned char* data = NULL;
    size_t data_len;
    if (COAP_RESPONSE_CLASS(received->hdr->code) == 2) {
        if (coap_get_data(received, &data_len, &data)) {
            printf("Received: %s\n", data);
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
    const char*       server_uri = COAP_DEFAULT_DEMO_URI;

    const int URI_HOSTNAME_SIZE = 64;
    char uri_hostname[URI_HOSTNAME_SIZE];

    const int BUFSIZE = 64;
    unsigned char _buf[BUFSIZE];
    unsigned char *buf = _buf;
    size_t buflen;


    while (1) {
        /* Wait for the callback to set the CONNECTED_BIT in the
           event group.
        */
        xEventGroupWaitBits(wifi_event_group, CONNECTED_BIT,
                            false, true, portMAX_DELAY);
        ESP_LOGI(TAG, "Connected to AP");

        if (coap_split_uri((const uint8_t *)server_uri, strlen(server_uri), &uri) == -1) {
            ESP_LOGE(TAG, "CoAP server uri error");
            break;
        }
        if (uri.host.length >= URI_HOSTNAME_SIZE) {
            ESP_LOGE(TAG, "CoAP server uri host length to big");
            return;
        }

        strncpy(uri_hostname, (const char *)uri.host.s, uri.host.length);
        uri_hostname[uri.host.length] = 0;
        hp = gethostbyname(uri_hostname);

        if (hp == NULL) {
            ESP_LOGE(TAG, "DNS lookup failed for %s", uri.host.s);
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
                    int res = coap_split_path(uri.path.s, uri.path.length, buf, &buflen);

                    while (res--)
                    {
                        coap_add_option(request, COAP_OPTION_URI_PATH,
                                                 COAP_OPT_LENGTH(buf),
                                                 COAP_OPT_VALUE(buf));

                        buf += COAP_OPT_SIZE(buf);
                    }
                }
            
                const char* request_data     = "Hello World!";
                coap_add_data  (request, strlen(request_data), (unsigned char *)request_data);

                coap_register_response_handler(ctx, message_handler);
                coap_send_confirmed(ctx, ctx->endpoint, &dst_addr, request);

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
                    } else if (result < 0) {
                        break;
                    } else {
                        ESP_LOGE(TAG, "select timeout");
                    }
                }
            }
            coap_free_context(ctx);
        }
    }

    vTaskDelete(NULL);
}
