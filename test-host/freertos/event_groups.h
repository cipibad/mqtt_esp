#ifndef EVENT_GROUPS_H
#define EVENT_GROUPS_H

typedef void * TimerHandle_t;

typedef unsigned long TickType_t;
typedef int BaseType_t;
typedef unsigned int UBaseType_t;
typedef void (*TimerCallbackFunction_t)( TimerHandle_t xTimer );

#define portTICK_PERIOD_MS 1234
#define pdPASS 0
#define pdTRUE 1
#define pdFALSE 1
#define portMAX_DELAY 9876


TimerHandle_t xTimerCreate(	const char * const pcTimerName,
								const TickType_t xTimerPeriodInTicks,
								const UBaseType_t uxAutoReload,
								void * const pvTimerID,
                            TimerCallbackFunction_t pxCallbackFunction );

#define pdMS_TO_TICKS( xTimeInMs ) xTimeInMs
BaseType_t xTimerStart( TimerHandle_t xTimer, const TickType_t xTicksToWait );


#endif /* EVENT_GROUPS_H */
