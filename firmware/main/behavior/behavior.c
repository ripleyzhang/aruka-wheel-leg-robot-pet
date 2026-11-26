
#include "behavior.h"

#include <stdbool.h>

#include "esp_log.h"
#include "expression/expression.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"

static const char * TAG = "ARUKA_BEHAVIOR";

#define EVENT_QUEUE_LENGTH 10
#define REQUEST_COUNT 3

static QueueHandle_t s_event_queue = NULL;

// Only the Behavior Task modifies these variables.
static aruka_wish_state_t s_wish_state = ARUKA_WISH_NONE;

static aruka_interaction_t s_interaction = ARUKA_INTERACTION_WAITING_FOR_WAKE;

static bool s_task_completed[REQUEST_COUNT] = {false};

// Helpers
static const char * wish_state_name(aruka_wish_state_t state) {
  switch (state) {
    case ARUKA_WISH_NONE:
      return "NONE";
    case ARUKA_WISH_REQUESTING:
      return "REQUESTING";
    case ARUKA_WISH_READY:
      return "WISH_READY";
    case ARUKA_WISH_GRANTED:
      return "WISH_GRANTED";
    default:
      return "UNKNOWN";
  }
}

static aruka_persona_t current_persona(void) {
  if (s_wish_state == ARUKA_WISH_READY || s_wish_state == ARUKA_WISH_GRANTED) {
    return ARUKA_PERSONA_NANIKA;
  }

  return ARUKA_PERSONA_ALLUKA;
}

static int completed_count(void) {
  int count = 0;

  for (int i = 0; i < REQUEST_COUNT; i++) {
    if (s_task_completed[i]) {
      count++;
    }
  }

  return count;
}

static void log_status(void) {
  ESP_LOGI(
      TAG,
      "Wish=%s | Persona=%s | Completed=%d/3 | Voice=%s",
      wish_state_name(s_wish_state),
      current_persona() == ARUKA_PERSONA_ALLUKA ? "ALLUKA" : "NANIKA",
      completed_count(),
      s_interaction == ARUKA_INTERACTION_LISTENING ? "LISTENING" : "WAITING_FOR_WAKE");
}

// Temporary display adapter for INT-02.
// Persona-specific faces will be implemented in EXP-03.
static void show_expression(aruka_expression_t expression) {
  esp_err_t ret = aruka_set_expression(expression);

  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "Display update failed: %s", esp_err_to_name(ret));
  }
}

static void reset_task_progress(void) {
  for (int i = 0; i < REQUEST_COUNT; i++) {
    s_task_completed[i] = false;
  }
}

// Queue publishing
static esp_err_t post_event(aruka_event_t event) {
  if (s_event_queue == NULL) {
    return ESP_ERR_INVALID_STATE;
  }

  if (event.type <= ARUKA_EVENT_NONE || event.type >= ARUKA_EVENT_COUNT) {
    return ESP_ERR_INVALID_ARG;
  }

  if (xQueueSend(s_event_queue, &event, 0) != pdTRUE) {
    ESP_LOGW(TAG, "Event queue full");
    return ESP_ERR_TIMEOUT;
  }

  return ESP_OK;
}

esp_err_t aruka_behavior_post_event(aruka_event_type_t type) {
  if (type == ARUKA_EVENT_TASK_CONFIRMED) {
    // This event requires a specific task ID.
    return ESP_ERR_INVALID_ARG;
  }

  aruka_event_t event = {.type = type, .task_id = 0};

  return post_event(event);
}

esp_err_t aruka_behavior_post_task_confirmed(uint8_t task_id) {
  if (task_id < 1 || task_id > REQUEST_COUNT) {
    return ESP_ERR_INVALID_ARG;
  }

  aruka_event_t event = {.type = ARUKA_EVENT_TASK_CONFIRMED, .task_id = task_id};

  return post_event(event);
}

// Wish Cycle
static void handle_task_confirmed(uint8_t task_id) {
  if (s_wish_state != ARUKA_WISH_REQUESTING) {
    ESP_LOGW(TAG, "Task ignored: no active requesting cycle");
    return;
  }

  if (task_id < 1 || task_id > REQUEST_COUNT) {
    ESP_LOGW(TAG, "Invalid task ID: %u", task_id);
    return;
  }

  int index = task_id - 1;

  // Reject duplicate confirmations.
  if (s_task_completed[index]) {
    ESP_LOGW(TAG, "Task %u already completed; ignoring duplicate", task_id);
    return;
  }

  s_task_completed[index] = true;

  ESP_LOGI(TAG, "Task %u confirmed!", task_id);
  ESP_LOGI(TAG, "Progress: %d/3", completed_count());

  if (completed_count() == REQUEST_COUNT) {
    // Persona automatically becomes NANIKA.
    s_wish_state = ARUKA_WISH_READY;

    ESP_LOGI(TAG, "ALLUKA -> NANIKA");
    ESP_LOGI(TAG, "All three requests fulfilled!");
    ESP_LOGI(TAG, "WISH READY: reward can now be claimed");

    // EXP-03 will implement the actual black-face display.
  }
  else {
    ESP_LOGI(TAG, "ALLUKA: Thank you!");
    // EXP-04 will trigger temporary DELIGHTED expression.
  }
}

// Main event handler
static void handle_event(aruka_event_t event) {
  switch (event.type) {
    case ARUKA_EVENT_CYCLE_START:
      if (s_wish_state != ARUKA_WISH_NONE) {
        ESP_LOGW(TAG, "Cycle already active; ignoring");
        break;
      }

      reset_task_progress();
      s_wish_state = ARUKA_WISH_REQUESTING;

      ESP_LOGI(TAG, "New Wish Cycle started");
      ESP_LOGI(TAG, "Persona: ALLUKA");
      break;

    case ARUKA_EVENT_TASK_CONFIRMED:
      handle_task_confirmed(event.task_id);
      break;

    case ARUKA_EVENT_REWARD_CLAIMED:
      if (s_wish_state != ARUKA_WISH_READY) {
        ESP_LOGW(TAG, "Reward not ready; claim ignored");
        break;
      }

      s_wish_state = ARUKA_WISH_GRANTED;

      ESP_LOGI(TAG, "NANIKA: Wish granted!");
      ESP_LOGI(TAG, "Reward marked as claimed");
      break;

    case ARUKA_EVENT_PRAISE_RECEIVED:
      if (s_wish_state != ARUKA_WISH_GRANTED) {
        ESP_LOGW(TAG, "Praise received outside reward phase");
        break;
      }

      ESP_LOGI(TAG, "NANIKA: Thank you!");
      ESP_LOGI(TAG, "NANIKA -> ALLUKA");

      s_wish_state = ARUKA_WISH_NONE;
      reset_task_progress();

      if (s_interaction == ARUKA_INTERACTION_WAITING_FOR_WAKE) {
        show_expression(ARUKA_EXPR_IDLE);
      }
      break;

    case ARUKA_EVENT_WAKE_WORD:
      s_interaction = ARUKA_INTERACTION_LISTENING;

      show_expression(ARUKA_EXPR_LISTENING);

      ESP_LOGI(TAG, "Voice interaction started");
      break;

    case ARUKA_EVENT_COMMAND_TIMEOUT:
      if (s_interaction != ARUKA_INTERACTION_LISTENING) {
        ESP_LOGW(TAG, "Timeout ignored: not listening");
        break;
      }

      s_interaction = ARUKA_INTERACTION_WAITING_FOR_WAKE;

      // Temporary behavior: keep the original IDLE face.
      // EXP-04 will restore the active persona's base face.
      show_expression(ARUKA_EXPR_IDLE);

      ESP_LOGI(TAG, "Voice interaction ended");
      break;

    // Legacy VOC-03 emotion commands are deprecated.
    // Do not let them control Wish Cycle or emotions.
    case ARUKA_EVENT_CMD_HAPPY:
    case ARUKA_EVENT_CMD_ANGRY:
    case ARUKA_EVENT_CMD_SLEEP:
    case ARUKA_EVENT_CMD_WAKE_UP:
      ESP_LOGW(TAG, "Legacy emotion command ignored");

      // VOC-03 is still single-command mode.
      // End the temporary listening phase.
      if (s_interaction == ARUKA_INTERACTION_LISTENING) {
        s_interaction = ARUKA_INTERACTION_WAITING_FOR_WAKE;
        show_expression(ARUKA_EXPR_IDLE);
      }
      break;

    default:
      ESP_LOGW(TAG, "Unknown event: %d", event.type);
      break;
  }

  log_status();
}

// Behavior Task
static void behavior_task(void * arg) {
  aruka_event_t event;

  ESP_LOGI(TAG, "Behavior task started");

  while (1) {
    if (xQueueReceive(s_event_queue, &event, portMAX_DELAY) == pdTRUE) {
      handle_event(event);
    }
  }
}

// Initialization
esp_err_t aruka_behavior_init(void) {
  if (s_event_queue != NULL) {
    return ESP_ERR_INVALID_STATE;
  }

  s_wish_state = ARUKA_WISH_NONE;
  s_interaction = ARUKA_INTERACTION_WAITING_FOR_WAKE;
  reset_task_progress();

  s_event_queue = xQueueCreate(EVENT_QUEUE_LENGTH, sizeof(aruka_event_t));

  if (s_event_queue == NULL) {
    ESP_LOGE(TAG, "Failed to create event queue");
    return ESP_ERR_NO_MEM;
  }

  BaseType_t result = xTaskCreate(behavior_task, "aruka_behavior", 4096, NULL, 5, NULL);

  if (result != pdPASS) {
    vQueueDelete(s_event_queue);
    s_event_queue = NULL;
    return ESP_ERR_NO_MEM;
  }

  ESP_LOGI(TAG, "Wish Cycle controller initialized");
  return ESP_OK;
}
