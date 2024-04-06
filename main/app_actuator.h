#ifndef APP_ACTUATOR_H
#define APP_ACTUATOR_H

typedef enum ActuatorStatus {
  ACTUATOR_STATUS_INIT,
  ACTUATOR_STATUS_OFF,
  ACTUATOR_STATUS_OPEN_TRANSITION,
  ACTUATOR_STATUS_CLOSE_TRANSITION
} ActuatorStatus_t;

#define GPIO_STATUS_OFF 0
#define GPIO_STATUS_ON 1

extern ActuatorStatus_t actuatorStatus;
ActuatorStatus_t inline getActuatorStatus() { return actuatorStatus; }

typedef enum ActuatorPeriod {
  ACTUATOR_PERIOD_UNSET = -1,
  ACTUATOR_PERIOD_NONE = 0,
  ACTUATOR_PERIOD_UNIT = (5 * 1000),
  ACTUATOR_PERIOD_CLOSE_RESERVE = (ACTUATOR_PERIOD_UNIT / 2),
  ACTUATOR_PERIOD_INTER_RELAY_OPERATION = (1 * 1000),
  ACTUATOR_PERIOD_QUARTER = (1 * ACTUATOR_PERIOD_UNIT),
  ACTUATOR_PERIOD_HALF = (2 * ACTUATOR_PERIOD_UNIT),
  ACTUATOR_PERIOD_THREE_QUARTERS = (3 * ACTUATOR_PERIOD_UNIT),
  ACTUATOR_PERIOD_FULL = (4 * ACTUATOR_PERIOD_UNIT),
} ActuatorPeriod_t;

typedef enum ActuatorLevel {
  ACTUATOR_LEVEL_UNSET = -1,
  ACTUATOR_LEVEL_CLOSED = 0,
  ACTUATOR_LEVEL_OPEN_1_4 = 1,
  ACTUATOR_LEVEL_OPEN_1_2 = 2,
  ACTUATOR_LEVEL_OPEN_3_4 = 3,
  ACTUATOR_LEVEL_OPEN = 4,
} ActuatorLevel_t;

typedef enum ActuatorCommand {
  ACTUATOR_COMMAND_UNSET,
  ACTUATOR_COMMAND_OPEN,
  ACTUATOR_COMMAND_CLOSE,
} ActuatorCommand_t;

void initActuator();
void updateActuatorState(ActuatorCommand_t command, ActuatorPeriod_t period);

#endif  // APP_ACTUATOR_H