#include "esp_system.h"
#ifdef CONFIG_NORTH_INTERFACE_UDP

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

#include "app_udp_common.h"

#include "esp_log.h"

#include <string.h>

extern QueueHandle_t intMsgQueue;
static const char *TAG = "udp_common";

void udp_publish_data(const char * topic,
                       const char * data,
                       int qos, int retain) {

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
    if (xQueueSend( intMsgQueue
                    ,( void * )&imsg
                    , INT_QUEUE_TIMEOUT) != pdPASS) {
      ESP_LOGE(TAG, "Cannot send to intMsgQueue");
    }
}
#endif // CONFIG_NORTH_INTERFACE_UDP
