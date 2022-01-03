#ifndef QUEUE_H
#define QUEUE_H


#include "event_groups.h"

typedef void * QueueHandle_t;
#define	queueSEND_TO_BACK		( ( BaseType_t ) 0 )


int xQueueSend( QueueHandle_t xQueue, const void * const pvItemToQueue, TickType_t xTicksToWait);
BaseType_t xQueueReceive( QueueHandle_t xQueue, void * const pvBuffer, TickType_t xTicksToWait);
BaseType_t xQueueSemaphoreTake( QueueHandle_t xQueue, TickType_t xTicksToWait );
BaseType_t xQueueGenericSend( QueueHandle_t xQueue, const void * const pvItemToQueue, TickType_t xTicksToWait, const BaseType_t xCopyPosition );
#endif /* QUEUE_H */

