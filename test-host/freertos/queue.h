typedef unsigned long TickType_t;
typedef int BaseType_t;
typedef unsigned int UBaseType_t;
typedef void (*TimerCallbackFunction_t)( TimerHandle_t xTimer );


#define portTICK_PERIOD_MS 1234
#define pdPASS 0
#define pdTRUE 1
#define pdFALSE 1
#define portMAX_DELAY 9876

int xQueueSend( QueueHandle_t xQueue, const void * const pvItemToQueue, TickType_t xTicksToWait);

TimerHandle_t xTimerCreate(	const char * const pcTimerName,
								const TickType_t xTimerPeriodInTicks,
								const UBaseType_t uxAutoReload,
								void * const pvTimerID,
                            TimerCallbackFunction_t pxCallbackFunction );

#define pdMS_TO_TICKS( xTimeInMs ) xTimeInMs
BaseType_t xTimerStart( TimerHandle_t xTimer, const TickType_t xTicksToWait );

BaseType_t xQueueReceive( QueueHandle_t xQueue, void * const pvBuffer, TickType_t xTicksToWait);

esp_err_t write_nvs_integer(const char * tag, int value);
esp_err_t read_nvs_integer(const char * tag, int * value);

