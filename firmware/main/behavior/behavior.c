#include "behavior.h"

#include "esp_log.h"
#include "expression/expression.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"

static const char * TAG = "ARUKA_BEHAVIOR";

#define EVENT_QUEUE_LENGTH 10

static QueueHandle_t s_event_queue = NULL;

esp_err_t aruka_behavior_post_event(aruka_event_type_t type) {
  if (s_event_queue == NULL) {
    ESP_LOGE(TAG, "Event queue not initialized");
    return ESP_ERR_INVALID_STATE;
  }

  if (type <= ARUKA_EVENT_NONE || type >= ARUKA_EVENT_COUNT) {
    ESP_LOGE(TAG, "Invalid event type: %d", type);
    return ESP_ERR_INVALID_ARG;
  }

  aruka_event_t event = {.type = type};

  BaseType_t result = xQueueSend(s_event_queue, &event, 0);

  if (result != pdTRUE) {
    ESP_LOGW(TAG, "Event queue full, event dropped: %d", type);
    return ESP_ERR_NO_MEM;
  }

  ESP_LOGI(TAG, "Event posted: %d", type);

  return ESP_OK;
}

static void behavior_task(void * arg) {
  aruka_event_t event;

  ESP_LOGI(TAG, "Behavior task started");

  while (1) {
    // Block here until an event arrives.
    if (xQueueReceive(s_event_queue, &event, portMAX_DELAY) == pdTRUE) {
      ESP_LOGI(TAG, "Processing event: %d", event.type);

      switch (event.type) {
        case ARUKA_EVENT_WAKE_WORD:
          ESP_LOGI(TAG, "Wake word event");
          aruka_set_expression(ARUKA_EXPR_LISTENING);
          break;

        case ARUKA_EVENT_CMD_HAPPY:
          ESP_LOGI(TAG, "Happy command");
          aruka_set_expression(ARUKA_EXPR_HAPPY);
          break;

        case ARUKA_EVENT_CMD_ANGRY:
          ESP_LOGI(TAG, "Angry command");
          aruka_set_expression(ARUKA_EXPR_ANGRY);
          break;

        case ARUKA_EVENT_CMD_SLEEP:
          ESP_LOGI(TAG, "Sleep command");
          aruka_set_expression(ARUKA_EXPR_SLEEPY);
          break;

        case ARUKA_EVENT_CMD_WAKE_UP:
          ESP_LOGI(TAG, "Wake-up command");
          aruka_set_expression(ARUKA_EXPR_IDLE);
          break;
        case ARUKA_EVENT_COMMAND_TIMEOUT:
          ESP_LOGI(TAG, "Command timeout");
          aruka_set_expression(ARUKA_EXPR_IDLE);
          break;

        default:
          ESP_LOGW(TAG, "Unhandled event: %d", event.type);
          break;
      }
    }
  }
}

esp_err_t aruka_behavior_init(void) {
  ESP_LOGI(TAG, "Initializing behavior system...");

  if (s_event_queue != NULL) {
    ESP_LOGW(TAG, "Behavior system already initialized");
    return ESP_ERR_INVALID_STATE;
  }

  s_event_queue = xQueueCreate(EVENT_QUEUE_LENGTH, sizeof(aruka_event_t));

  if (s_event_queue == NULL) {
    ESP_LOGE(TAG, "Failed to create event queue");
    return ESP_ERR_NO_MEM;
  }

  BaseType_t task_result =
      xTaskCreate(behavior_task, "aruka_behavior", 4096, NULL, 5, NULL);

  if (task_result != pdPASS) {
    ESP_LOGE(TAG, "Failed to create behavior task");

    vQueueDelete(s_event_queue);
    s_event_queue = NULL;

    return ESP_ERR_NO_MEM;
  }

  ESP_LOGI(TAG, "Behavior system initialized");

  return ESP_OK;
}
