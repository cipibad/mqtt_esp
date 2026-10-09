#ifndef TASK_H
#define TASK_H

#include "FreeRTOS.h"

typedef void (*TaskFunction_t)( void * );
typedef void * TaskHandle_t;

void xTaskCreate( TaskFunction_t pvTaskCode, const char * const pcName,
                  unsigned int usStackDepth, void * const pvParameters,
                  unsigned int uxPriority, TaskHandle_t * const pxCreatedTask );

#endif /* TASK_H */
