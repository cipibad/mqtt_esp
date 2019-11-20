#ifndef QUEUE_H
#define QUEUE_H


#include "event_groups.h"

typedef void * QueueHandle_t;


int xQueueSend( QueueHandle_t xQueue, const void * const pvItemToQueue, TickType_t xTicksToWait);
BaseType_t xQueueReceive( QueueHandle_t xQueue, void * const pvBuffer, TickType_t xTicksToWait);

#endif /* QUEUE_H */

