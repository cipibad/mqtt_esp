#ifndef EVENT_GROUPS_H
#define EVENT_GROUPS_H

typedef void * TimerHandle_t;
typedef void * EventGroupHandle_t;

typedef unsigned long TickType_t;
typedef int BaseType_t;
typedef unsigned int UBaseType_t;
typedef void (*TimerCallbackFunction_t)( TimerHandle_t xTimer );

#define portTICK_PERIOD_MS 1234
#define pdPASS 0
#define pdTRUE 1
#define pdFALSE 1
#define portMAX_DELAY 9876

#define ESP_OK 0

TimerHandle_t xTimerCreate(	const char * const pcTimerName,
								const TickType_t xTimerPeriodInTicks,
								const UBaseType_t uxAutoReload,
								void * const pvTimerID,
                            TimerCallbackFunction_t pxCallbackFunction );

#define pdMS_TO_TICKS( xTimeInMs ) xTimeInMs
BaseType_t xTimerStart( TimerHandle_t xTimer, const TickType_t xTicksToWait );



#define BIT31   0x80000000
#define BIT30   0x40000000
#define BIT29   0x20000000
#define BIT28   0x10000000
#define BIT27   0x08000000
#define BIT26   0x04000000
#define BIT25   0x02000000
#define BIT24   0x01000000
#define BIT23   0x00800000
#define BIT22   0x00400000
#define BIT21   0x00200000
#define BIT20   0x00100000
#define BIT19   0x00080000
#define BIT18   0x00040000
#define BIT17   0x00020000
#define BIT16   0x00010000
#define BIT15   0x00008000
#define BIT14   0x00004000
#define BIT13   0x00002000
#define BIT12   0x00001000
#define BIT11   0x00000800
#define BIT10   0x00000400
#define BIT9     0x00000200
#define BIT8     0x00000100
#define BIT7     0x00000080
#define BIT6     0x00000040
#define BIT5     0x00000020
#define BIT4     0x00000010
#define BIT3     0x00000008
#define BIT2     0x00000004
#define BIT1     0x00000002
#define BIT0     0x00000001


typedef TickType_t EventBits_t;
EventBits_t xEventGroupGetBits( EventGroupHandle_t xEventGroup );
EventBits_t xEventGroupClearBits( EventGroupHandle_t xEventGroup, const EventBits_t uxBitsToClear );
EventBits_t xEventGroupWaitBits( EventGroupHandle_t xEventGroup, const EventBits_t uxBitsToWaitFor, const BaseType_t xClearOnExit, const BaseType_t xWaitForAllBits, TickType_t xTicksToWait );
EventBits_t xEventGroupSetBits( EventGroupHandle_t xEventGroup, const EventBits_t uxBitsToSet );

void vTaskDelay(int);
int esp_reset_reason();

#endif /* EVENT_GROUPS_H */

