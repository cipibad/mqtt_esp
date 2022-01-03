#ifndef SEMPHR_H
#define SEMPHR_H

#include "queue.h"

#define semGIVE_BLOCK_TIME					( ( TickType_t ) 0U )

typedef QueueHandle_t SemaphoreHandle_t;

#define xSemaphoreTake( xSemaphore, xBlockTime )		xQueueSemaphoreTake( ( xSemaphore ), ( xBlockTime ) )
#define xSemaphoreGive( xSemaphore )		xQueueGenericSend( ( QueueHandle_t ) ( xSemaphore ), NULL, semGIVE_BLOCK_TIME, queueSEND_TO_BACK )

#endif // !SEMPHR_H
