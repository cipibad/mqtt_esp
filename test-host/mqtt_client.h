#ifndef MQTT_CLIENT_H
#define MQTT_CLIENT_H

#include "freertos/FreeRTOS.h"

typedef void * esp_mqtt_client_handle_t;


typedef enum {
    MQTT_EVENT_ERROR = 0,
    MQTT_EVENT_CONNECTED,
    MQTT_EVENT_DISCONNECTED,
    MQTT_EVENT_SUBSCRIBED,
    MQTT_EVENT_UNSUBSCRIBED,
    MQTT_EVENT_PUBLISHED,
    MQTT_EVENT_DATA,
    MQTT_EVENT_BEFORE_CONNECT,
} esp_mqtt_event_id_t;


/**
 * MQTT event configuration structure
 */
typedef struct {
    esp_mqtt_event_id_t event_id;       /*!< MQTT event type */
    esp_mqtt_client_handle_t client;    /*!< MQTT client handle for this event */
    void *user_context;                 /*!< User context passed from MQTT client config */
    char *data;                         /*!< Data asociated with this event */
    int data_len;                       /*!< Lenght of the data for this event */
    int total_data_len;                 /*!< Total length of the data (longer data are supplied with multiple events) */
    int current_data_offset;            /*!< Actual offset for the data asociated with this event */
    char *topic;                        /*!< Topic asociated with this event */
    int topic_len;                      /*!< Length of the topic for this event asociated with this event */
    int msg_id;                         /*!< MQTT messaged id of message */
} esp_mqtt_event_t;

typedef esp_mqtt_event_t* esp_mqtt_event_handle_t;


int esp_mqtt_client_publish(esp_mqtt_client_handle_t client, const char *topic, const char *data, int len, int qos, int retain);

esp_err_t esp_mqtt_client_subscribe(esp_mqtt_client_handle_t client, const char *topic, int qos);


typedef esp_err_t (* mqtt_event_callback_t)(esp_mqtt_event_handle_t event);

typedef enum {
    MQTT_TRANSPORT_UNKNOWN = 0x0,
    MQTT_TRANSPORT_OVER_TCP,      /*!< MQTT over TCP, using scheme: ``mqtt`` */
    MQTT_TRANSPORT_OVER_SSL,      /*!< MQTT over SSL, using scheme: ``mqtts`` */
    MQTT_TRANSPORT_OVER_WS,       /*!< MQTT over Websocket, using scheme:: ``ws`` */
    MQTT_TRANSPORT_OVER_WSS       /*!< MQTT over Websocket Secure, using scheme: ``wss`` */
} esp_mqtt_transport_t;

typedef struct {
    mqtt_event_callback_t event_handle;     /*!< handle for MQTT events */
    const char *host;                       /*!< MQTT server domain (ipv4 as string) */
    const char *uri;                        /*!< Complete MQTT broker URI */
    unsigned int port;                          /*!< MQTT server port */
    const char *client_id;                  /*!< default client id is ``ESP32_%CHIPID%`` where %CHIPID% are last 3 bytes of MAC address in hex format */
    const char *username;                   /*!< MQTT username */
    const char *password;                   /*!< MQTT password */
    const char *lwt_topic;                  /*!< LWT (Last Will and Testament) message topic (NULL by default) */
    const char *lwt_msg;                    /*!< LWT message (NULL by default) */
    int lwt_qos;                            /*!< LWT message qos */
    int lwt_retain;                         /*!< LWT retained message flag */
    int lwt_msg_len;                        /*!< LWT message length */
    int disable_clean_session;              /*!< mqtt clean session, default clean_session is true */
    int keepalive;                          /*!< mqtt keepalive, default is 120 seconds */
    bool disable_auto_reconnect;            /*!< this mqtt client will reconnect to server (when errors/disconnect). Set disable_auto_reconnect=true to disable */
    void *user_context;                     /*!< pass user context to this option, then can receive that context in ``event->user_context`` */
    int task_prio;                          /*!< MQTT task priority, default is 5, can be changed in ``make menuconfig`` */
    int task_stack;                         /*!< MQTT task stack size, default is 6144 bytes, can be changed in ``make menuconfig`` */
    int buffer_size;                        /*!< size of MQTT send/receive buffer, default is 1024 */
    const char *cert_pem;                   /*!< pointer to CERT file for server verify (with SSL), default is NULL, not required to verify the server */
    esp_mqtt_transport_t transport;         /*!< overrides URI transport */
} esp_mqtt_client_config_t;


esp_mqtt_client_handle_t esp_mqtt_client_init(const esp_mqtt_client_config_t *config);
esp_err_t esp_mqtt_client_start(esp_mqtt_client_handle_t client);

#endif /* MQTT_CLIENT_H */
