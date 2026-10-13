#ifndef MQTT_CLIENT_H
#define MQTT_CLIENT_H

#include <stdint.h>
#include <stdbool.h>

#include "freertos/FreeRTOS.h"

typedef void * esp_mqtt_client_handle_t;
typedef const char * esp_event_base_t;

typedef enum {
    MQTT_EVENT_ANY = -1,
    MQTT_EVENT_ERROR = 0,
    MQTT_EVENT_BEFORE_CONNECT,
    MQTT_EVENT_CONNECTED,
    MQTT_EVENT_DISCONNECTED,
    MQTT_EVENT_SUBSCRIBED,
    MQTT_EVENT_UNSUBSCRIBED,
    MQTT_EVENT_PUBLISHED,
    MQTT_EVENT_DATA,
    MQTT_EVENT_DELETED,
} esp_mqtt_event_id_t;


/**
 * MQTT event structure
 */
typedef struct {
    esp_mqtt_event_id_t event_id;       /*!< MQTT event type */
    esp_mqtt_client_handle_t client;    /*!< MQTT client handle for this event */
    void *user_context;                 /*!< User context passed from MQTT client config */
    char *data;                         /*!< Data associated with this event */
    int data_len;                       /*!< Length of the data for this event */
    int total_data_len;                 /*!< Total length of the data (longer data are supplied with multiple events) */
    int current_data_offset;            /*!< Actual offset for the data associated with this event */
    char *topic;                        /*!< Topic associated with this event */
    int topic_len;                      /*!< Length of the topic for this event */
    int msg_id;                         /*!< MQTT message id of message */
} esp_mqtt_event_t;

typedef esp_mqtt_event_t* esp_mqtt_event_handle_t;

typedef void (* esp_event_handler_t)(void *handler_arg, esp_event_base_t base, int32_t id, void *event_data);

typedef enum {
    MQTT_TRANSPORT_UNKNOWN = 0x0,
    MQTT_TRANSPORT_OVER_TCP,      /*!< MQTT over TCP, using scheme: ``mqtt`` */
    MQTT_TRANSPORT_OVER_SSL,      /*!< MQTT over SSL, using scheme: ``mqtts`` */
    MQTT_TRANSPORT_OVER_WS,       /*!< MQTT over Websocket, using scheme:: ``ws`` */
    MQTT_TRANSPORT_OVER_WSS       /*!< MQTT over Websocket Secure, using scheme: ``wss`` */
} esp_mqtt_transport_t;

typedef struct {
    struct {
        const char *uri;
    } address;
    struct {
        const char *certificate;
    } verification;
} mqtt_config_broker_t;

typedef struct {
    struct {
        const char *password;
    } authentication;
    const char *username;
    const char *client_id;
} mqtt_config_credentials_t;

typedef struct {
    int keepalive;
    struct {
        const char *topic;
        const char *msg;
        int msg_len;
        int qos;
        bool retain;
    } last_will;
    bool disable_clean_session;
} mqtt_config_session_t;

/**
 * MQTT event configuration structure (subset of the esp-idf v6 layout
 * used by app_mqtt.c)
 */
typedef struct {
    mqtt_config_broker_t broker;
    mqtt_config_credentials_t credentials;
    mqtt_config_session_t session;
    struct {
        int size;
        int out_size;
    } buffer;
} esp_mqtt_client_config_t;


int esp_mqtt_client_publish(esp_mqtt_client_handle_t client, const char *topic, const char *data, int len, int qos, int retain);

esp_err_t esp_mqtt_client_subscribe(esp_mqtt_client_handle_t client, const char *topic, int qos);

typedef enum {
    MQTT_CLIENT_STATE_NOT_INITIALIZED = 0,
    MQTT_CLIENT_STATE_NOT_STARTED,
    MQTT_CLIENT_STATE_DISCONNECTED,
    MQTT_CLIENT_STATE_CONNECTED,
    MQTT_CLIENT_STATE_WAITING_RECONNECT,
} esp_mqtt_client_connection_state_t;

esp_mqtt_client_handle_t esp_mqtt_client_init(const esp_mqtt_client_config_t *config);
esp_err_t esp_mqtt_client_start(esp_mqtt_client_handle_t client);
esp_err_t esp_mqtt_client_register_event(esp_mqtt_client_handle_t client, esp_mqtt_event_id_t event, esp_event_handler_t event_handler, void *event_handler_arg);
esp_mqtt_client_connection_state_t esp_mqtt_client_get_state(esp_mqtt_client_handle_t client);

#endif /* MQTT_CLIENT_H */
