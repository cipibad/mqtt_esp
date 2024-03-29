#include "esp_system.h"
#ifdef CONFIG_COAP_SERVER_SUPPORT

#include "app_coap.h"
#include "app_coap_server.h"

#include "esp_log.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"

#include "coap_config.h"
#include "coap.h"

#include "app_publish_data.h"


#if CONFIG_MQTT_THERMOSTATS_NB > 0
#include "app_thermostat.h"
#endif // CONFIG_MQTT_THERMOSTATS_NB > 0

#define COAP_DEFAULT_TIME_SEC 5
#define COAP_DEFAULT_TIME_USEC 0


extern EventGroupHandle_t wifi_event_group;
extern const int WIFI_CONNECTED_BIT;

static const char *TAG = "COAP_SERVER";
static coap_async_state_t *async = NULL;

static void
send_async_response(coap_context_t *ctx, const coap_endpoint_t *local_if)
{
    coap_pdu_t *response;
    unsigned char buf[3];
    size_t data_size = sizeof(coap_hdr_t) + 20;
    response = coap_pdu_init(async->flags & COAP_MESSAGE_CON, COAP_RESPONSE_CODE(205), 0, data_size);
    response->hdr->id = coap_new_message_id(ctx);
    if (async->tokenlen) {
        coap_add_token(response, async->tokenlen, async->token);
    }
    coap_add_option(response, COAP_OPTION_CONTENT_TYPE, coap_encode_var_bytes(buf, COAP_MEDIATYPE_TEXT_PLAIN), buf);

    if (coap_send(ctx, local_if, &async->peer, response) == COAP_INVALID_TID) {
        ESP_LOGI(TAG, "sending response for message id: %d", response->hdr->id);
    }
    coap_delete_pdu(response);
    coap_async_state_t *tmp;
    coap_remove_async(ctx, async->id, &tmp);
    coap_free_async(async);
    async = NULL;
}

/*
 * The PUT resource handler
 */
static void
async_handler_put(coap_context_t *ctx, struct coap_resource_t *resource,
              const coap_endpoint_t *local_interface, coap_address_t *peer,
              coap_pdu_t *request, str *token, coap_pdu_t *response)
{
    async = coap_register_async(ctx, peer, request, COAP_ASYNC_SEPARATE | COAP_ASYNC_CONFIRM, (void*)"no data");

    size_t coap_data_size;
    unsigned char * coap_data;
    char data[COAP_MAX_DATA_SIZE];
    char uri[COAP_MAX_RESOURCE_SIZE];

    /* coap_get_data() sets data_size to 0 on error */
    if (coap_get_data(request, &coap_data_size, &coap_data)){
        if (coap_data_size != 0) {
            memcpy(data, coap_data, coap_data_size);
            data[coap_data_size] = 0;
            ESP_LOGI(TAG, "received data for %.*s: %s",
                resource->uri.length, resource->uri.s, data);
            if (resource->uri.length >= COAP_MAX_RESOURCE_SIZE) {
                ESP_LOGE(TAG, "coap message path longer than expected, it will be ignored");
                return;
            }
            strncpy(uri, (const char *)resource->uri.s, resource->uri.length);
            uri[resource->uri.length] = 0;
            ESP_LOGI(TAG, "hadling path: %s", uri);
#ifdef CONFIG_MQTT_THERMOSTATS_NB0_COAP_SENSOR_RESOURCE
            if (strncmp(uri, CONFIG_MQTT_THERMOSTATS_NB0_COAP_SENSOR_RESOURCE,
                strlen(CONFIG_MQTT_THERMOSTATS_NB0_COAP_SENSOR_RESOURCE)) == 0) {
                int value = atof(data) * 10;
                thermostat_publish_local_data(0, value);
            }
#endif // CONFIG_MQTT_THERMOSTATS_NB0_COAP_SENSOR_RESOURCE
#ifdef CONFIG_MQTT_THERMOSTATS_NB1_COAP_SENSOR_RESOURCE
            if (strncmp(uri, CONFIG_MQTT_THERMOSTATS_NB1_COAP_SENSOR_RESOURCE,
                strlen(CONFIG_MQTT_THERMOSTATS_NB1_COAP_SENSOR_RESOURCE)) == 0) {
                int value = atof(data) * 10;
                thermostat_publish_local_data(1, value);
            }
#endif // CONFIG_MQTT_THERMOSTATS_NB1_COAP_SENSOR_RESOURCE
#ifdef CONFIG_MQTT_THERMOSTATS_NB2_COAP_SENSOR_RESOURCE
            if (strncmp(uri, CONFIG_MQTT_THERMOSTATS_NB2_COAP_SENSOR_RESOURCE,
                strlen(CONFIG_MQTT_THERMOSTATS_NB2_COAP_SENSOR_RESOURCE)) == 0) {
                int value = atof(data) * 10;
                thermostat_publish_local_data(2, value);
            }
#endif // CONFIG_MQTT_THERMOSTATS_NB2_COAP_SENSOR_RESOURCE
#ifdef CONFIG_MQTT_THERMOSTATS_NB3_COAP_SENSOR_RESOURCE
            if (strncmp(uri, CONFIG_MQTT_THERMOSTATS_NB3_COAP_SENSOR_RESOURCE,
                strlen(CONFIG_MQTT_THERMOSTATS_NB3_COAP_SENSOR_RESOURCE)) == 0) {
                int value = atof(data) * 10;
                thermostat_publish_local_data(3, value);
            }
#endif // CONFIG_MQTT_THERMOSTATS_NB3_COAP_SENSOR_RESOURCE
#ifdef CONFIG_MQTT_THERMOSTATS_NB4_COAP_SENSOR_RESOURCE
            if (strncmp(uri, CONFIG_MQTT_THERMOSTATS_NB4_COAP_SENSOR_RESOURCE,
                strlen(CONFIG_MQTT_THERMOSTATS_NB4_COAP_SENSOR_RESOURCE)) == 0) {
                int value = atof(data) * 10;
                thermostat_publish_local_data(4, value);
            }
#endif // CONFIG_MQTT_THERMOSTATS_NB4_COAP_SENSOR_RESOURCE
#ifdef CONFIG_MQTT_THERMOSTATS_NB5_COAP_SENSOR_RESOURCE
            if (strncmp(uri, CONFIG_MQTT_THERMOSTATS_NB5_COAP_SENSOR_RESOURCE,
                strlen(CONFIG_MQTT_THERMOSTATS_NB5_COAP_SENSOR_RESOURCE)) == 0) {
                int value = atof(data) * 10;
                thermostat_publish_local_data(5, value);
            }
#endif // CONFIG_MQTT_THERMOSTATS_NB5_COAP_SENSOR_RESOURCE
            publish_non_persistent_data(uri, data);
        }
    }
}

void coap_server_thread(void *p)
{
    coap_context_t*  ctx = NULL;
    coap_address_t   serv_addr;
    fd_set           readfds;
    struct timeval tv;
    int flags = 0;
    ESP_LOGI(TAG, "Starting thread");
    bool resource_registered = false;

    while (1) {
        /* Wait for the callback to set the CONNECTED_BIT in the
           event group.
        */
        xEventGroupWaitBits(wifi_event_group, WIFI_CONNECTED_BIT,
                            false, true, portMAX_DELAY);
        ESP_LOGI(TAG, "Connected to AP");

        /* Prepare the CoAP server socket */
        coap_address_init(&serv_addr);
        serv_addr.addr.sin.sin_family      = AF_INET;
        serv_addr.addr.sin.sin_addr.s_addr = INADDR_ANY;
        serv_addr.addr.sin.sin_port        = htons(COAP_DEFAULT_PORT);
        ctx                                = coap_new_context(&serv_addr);
        if (ctx) {
            flags = fcntl(ctx->sockfd, F_GETFL, 0);
            fcntl(ctx->sockfd, F_SETFL, flags|O_NONBLOCK);

            tv.tv_usec = COAP_DEFAULT_TIME_USEC;
            tv.tv_sec = COAP_DEFAULT_TIME_SEC;
            /* Initialize the resource */
            coap_resource_t* resource = NULL;
#ifdef CONFIG_MQTT_THERMOSTATS_NB0_COAP_SENSOR_RESOURCE
            resource = coap_resource_init((unsigned char *)CONFIG_MQTT_THERMOSTATS_NB0_COAP_SENSOR_RESOURCE,
                strlen(CONFIG_MQTT_THERMOSTATS_NB0_COAP_SENSOR_RESOURCE), 0);
            if (resource){
                ESP_LOGI(TAG, "registering resource " CONFIG_MQTT_THERMOSTATS_NB0_COAP_SENSOR_RESOURCE);
                coap_register_handler(resource, COAP_REQUEST_PUT, async_handler_put);
                coap_add_resource(ctx, resource);
                if (resource_registered == false) { resource_registered = true;}
            }
#endif // CONFIG_MQTT_THERMOSTATS_NB0_COAP_SENSOR_RESOURCE
#ifdef CONFIG_MQTT_THERMOSTATS_NB1_COAP_SENSOR_RESOURCE
            resource = coap_resource_init((unsigned char *)CONFIG_MQTT_THERMOSTATS_NB1_COAP_SENSOR_RESOURCE,
                strlen(CONFIG_MQTT_THERMOSTATS_NB1_COAP_SENSOR_RESOURCE), 0);
            if (resource){
                ESP_LOGI(TAG, "registering resource " CONFIG_MQTT_THERMOSTATS_NB1_COAP_SENSOR_RESOURCE);
                coap_register_handler(resource, COAP_REQUEST_PUT, async_handler_put);
                coap_add_resource(ctx, resource);
                if (resource_registered == false) { resource_registered = true;}
            }
#endif // CONFIG_MQTT_THERMOSTATS_NB1_COAP_SENSOR_RESOURCE
#ifdef CONFIG_MQTT_THERMOSTATS_NB2_COAP_SENSOR_RESOURCE
            resource = coap_resource_init((unsigned char *)CONFIG_MQTT_THERMOSTATS_NB2_COAP_SENSOR_RESOURCE,
                strlen(CONFIG_MQTT_THERMOSTATS_NB2_COAP_SENSOR_RESOURCE), 0);
            if (resource){
                ESP_LOGI(TAG, "registering resource " CONFIG_MQTT_THERMOSTATS_NB2_COAP_SENSOR_RESOURCE);
                coap_register_handler(resource, COAP_REQUEST_PUT, async_handler_put);
                coap_add_resource(ctx, resource);
                if (resource_registered == false) { resource_registered = true;}
            }
#endif // CONFIG_MQTT_THERMOSTATS_NB2_COAP_SENSOR_RESOURCE
#ifdef CONFIG_MQTT_THERMOSTATS_NB3_COAP_SENSOR_RESOURCE
            resource = coap_resource_init((unsigned char *)CONFIG_MQTT_THERMOSTATS_NB3_COAP_SENSOR_RESOURCE,
                strlen(CONFIG_MQTT_THERMOSTATS_NB3_COAP_SENSOR_RESOURCE), 0);
            if (resource){
                ESP_LOGI(TAG, "registering resource " CONFIG_MQTT_THERMOSTATS_NB3_COAP_SENSOR_RESOURCE);
                coap_register_handler(resource, COAP_REQUEST_PUT, async_handler_put);
                coap_add_resource(ctx, resource);
                if (resource_registered == false) { resource_registered = true;}
            }
#endif // CONFIG_MQTT_THERMOSTATS_NB3_COAP_SENSOR_RESOURCE
#ifdef CONFIG_MQTT_THERMOSTATS_NB4_COAP_SENSOR_RESOURCE
            resource = coap_resource_init((unsigned char *)CONFIG_MQTT_THERMOSTATS_NB4_COAP_SENSOR_RESOURCE,
                strlen(CONFIG_MQTT_THERMOSTATS_NB4_COAP_SENSOR_RESOURCE), 0);
            if (resource){
                ESP_LOGI(TAG, "registering resource " CONFIG_MQTT_THERMOSTATS_NB4_COAP_SENSOR_RESOURCE);
                coap_register_handler(resource, COAP_REQUEST_PUT, async_handler_put);
                coap_add_resource(ctx, resource);
                if (resource_registered == false) { resource_registered = true;}
            }
#endif // CONFIG_MQTT_THERMOSTATS_NB4_COAP_SENSOR_RESOURCE
#ifdef CONFIG_MQTT_THERMOSTATS_NB5_COAP_SENSOR_RESOURCE
            resource = coap_resource_init((unsigned char *)CONFIG_MQTT_THERMOSTATS_NB5_COAP_SENSOR_RESOURCE,
                strlen(CONFIG_MQTT_THERMOSTATS_NB5_COAP_SENSOR_RESOURCE), 0);
            if (resource){
                ESP_LOGI(TAG, "registering resource " CONFIG_MQTT_THERMOSTATS_NB5_COAP_SENSOR_RESOURCE);
                coap_register_handler(resource, COAP_REQUEST_PUT, async_handler_put);
                coap_add_resource(ctx, resource);
                if (resource_registered == false) { resource_registered = true;}
            }
#endif // CONFIG_MQTT_THERMOSTATS_NB5_COAP_SENSOR_RESOURCE

#if CONFIG_COAP_PROXY_PATHS_NB > 0
            resource = coap_resource_init((unsigned char *)CONFIG_COAP_PROXY_PATH_NB0,
                strlen(CONFIG_COAP_PROXY_PATH_NB0), 0);
            if (resource){
                ESP_LOGI(TAG, "registering resource " CONFIG_COAP_PROXY_PATH_NB0);
                coap_register_handler(resource, COAP_REQUEST_PUT, async_handler_put);
                coap_add_resource(ctx, resource);
                if (resource_registered == false) { resource_registered = true;}
            }

#if CONFIG_COAP_PROXY_PATHS_NB > 1
            resource = coap_resource_init((unsigned char *)CONFIG_COAP_PROXY_PATH_NB1,
                strlen(CONFIG_COAP_PROXY_PATH_NB1), 0);
            if (resource){
                ESP_LOGI(TAG, "registering resource " CONFIG_COAP_PROXY_PATH_NB1);
                coap_register_handler(resource, COAP_REQUEST_PUT, async_handler_put);
                coap_add_resource(ctx, resource);
                if (resource_registered == false) { resource_registered = true;}
            }

#endif // CONFIG_COAP_PROXY_PATHS_NB > 1
#endif // CONFIG_COAP_PROXY_PATHS_NB > 0
            if (resource_registered) {
                /*For incoming connections*/
                for (;;) {
                    FD_ZERO(&readfds);
                    FD_CLR( ctx->sockfd, &readfds);
                    FD_SET( ctx->sockfd, &readfds);

                    int result = select( ctx->sockfd+1, &readfds, 0, 0, &tv );
                    if (result > 0){
                        if (FD_ISSET( ctx->sockfd, &readfds ))
                            coap_read(ctx);
                    } else if (result < 0){
                        break;
                    } else {
                        ESP_LOGD(TAG, "select timeout");
                    }
                    if (async) {
                        send_async_response(ctx, ctx->endpoint);
                    }
                }
            }
            coap_free_context(ctx);
        }
    }

    vTaskDelete(NULL);
}

#endif // CONFIG_COAP_SERVER_SUPPORT