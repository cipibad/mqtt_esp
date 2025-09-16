#ifndef APP_VALVE_H
#define APP_VALVE_H

typedef enum valve_cmd {
    VALVE_CMD_UNSET = 0,
    VALVE_CMD_STATUS
} valve_cmd_t;

typedef enum valve_status {
    VALVE_STATUS_INIT = 0,
    VALVE_STATUS_CLOSED,
    VALVE_STATUS_CLOSED_OPEN_TRANSITION,
    VALVE_STATUS_OPEN,
    VALVE_STATUS_OPEN_CLOSED_TRANSITION
} valve_status_t;

struct ValveMessage {
  valve_cmd_t msgType;
  valve_status_t data;
};

void init_valve();
void publish_valve_status();

void app_valve_task(void* pvParameters);

#endif /* APP_VALVE_H */
