
#pragma once

#include <stdint.h>

#include "esp_err.h"

// Events received through the FreeRTOS queue.
typedef enum {
  ARUKA_EVENT_NONE = 0,

  // Existing voice events
  ARUKA_EVENT_WAKE_WORD,
  ARUKA_EVENT_CMD_HAPPY,    // Legacy, to be removed in VOC-04
  ARUKA_EVENT_CMD_ANGRY,    // Legacy
  ARUKA_EVENT_CMD_SLEEP,    // Legacy
  ARUKA_EVENT_CMD_WAKE_UP,  // Legacy
  ARUKA_EVENT_COMMAND_TIMEOUT,

  // New Wish Cycle events
  ARUKA_EVENT_CYCLE_START,
  ARUKA_EVENT_TASK_CONFIRMED,
  ARUKA_EVENT_REWARD_CLAIMED,
  ARUKA_EVENT_PRAISE_RECEIVED,

  ARUKA_EVENT_COUNT
} aruka_event_type_t;

// Event message.
// task_id is only used by TASK_CONFIRMED (1, 2, or 3).
typedef struct {
  aruka_event_type_t type;
  uint8_t task_id;
} aruka_event_t;

// Wish Cycle state.
typedef enum {
  ARUKA_WISH_NONE = 0,
  ARUKA_WISH_REQUESTING,
  ARUKA_WISH_READY,
  ARUKA_WISH_GRANTED
} aruka_wish_state_t;

// Persona is derived from Wish State.
typedef enum { ARUKA_PERSONA_ALLUKA = 0, ARUKA_PERSONA_NANIKA } aruka_persona_t;

// Voice interaction state.
typedef enum {
  ARUKA_INTERACTION_WAITING_FOR_WAKE = 0,
  ARUKA_INTERACTION_LISTENING
} aruka_interaction_t;

// Initialize the Behavior Controller.
esp_err_t aruka_behavior_init(void);

// Publish a simple event.
esp_err_t aruka_behavior_post_event(aruka_event_type_t type);

// Publish a confirmed task completion (task_id: 1-3).
// INT-02 uses this with mock data.
esp_err_t aruka_behavior_post_task_confirmed(uint8_t task_id);
