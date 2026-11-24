#pragma once

#include "esp_err.h"

typedef enum {
  ARUKA_EVENT_NONE = 0,

  // Voice events
  ARUKA_EVENT_WAKE_WORD,
  ARUKA_EVENT_CMD_HAPPY,
  ARUKA_EVENT_CMD_ANGRY,
  ARUKA_EVENT_CMD_SLEEP,
  ARUKA_EVENT_CMD_WAKE_UP,
  ARUKA_EVENT_COMMAND_TIMEOUT,

  ARUKA_EVENT_COUNT
} aruka_event_type_t;

typedef struct {
  aruka_event_type_t type;
} aruka_event_t;

// Initialize behavior controller and event queue
esp_err_t aruka_behavior_init(void);

// Send an event to the behavior controller
esp_err_t aruka_behavior_post_event(aruka_event_type_t type);
