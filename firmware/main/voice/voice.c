#include "voice.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

#include "behavior.h"
#include "bsp/esp-box-3.h"
#include "esp_afe_config.h"
#include "esp_afe_sr_iface.h"
#include "esp_afe_sr_models.h"
#include "esp_codec_dev.h"
#include "esp_log.h"
#include "esp_mn_iface.h"
#include "esp_mn_models.h"
#include "esp_mn_speech_commands.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "model_path.h"

#define SAMPLE_RATE 16000

static const char * TAG = "ARUKA_VOICE";

static esp_codec_dev_handle_t s_mic = NULL;
static const esp_afe_sr_iface_t * s_afe = NULL;
static esp_afe_sr_data_t * s_afe_data = NULL;
static srmodel_list_t * s_models = NULL;

// continuously capture microphone data.
static void audio_feed_task(void * arg) {
  const int chunk = s_afe->get_feed_chunksize(s_afe_data);
  const int channels = s_afe->get_feed_channel_num(s_afe_data);

  const size_t bytes = (size_t)chunk * channels * sizeof(int16_t);

  int16_t * buffer = malloc(bytes);

  if (!buffer) {
    ESP_LOGE(TAG, "Cannot allocate feed buffer");
    vTaskDelete(NULL);
    return;
  }

  ESP_LOGI(TAG, "Audio feed started: %d samples, %d channels", chunk, channels);

  while (1) {
    int ret = esp_codec_dev_read(s_mic, buffer, bytes);

    if (ret != 0) {
      ESP_LOGE(TAG, "Microphone read error: %d", ret);
      vTaskDelay(pdMS_TO_TICKS(100));
      continue;
    }

    s_afe->feed(s_afe_data, buffer);
  }
}

static esp_err_t init_multinet(const esp_mn_iface_t ** out_mn,
                               model_iface_data_t ** out_data) {
  // Find the selected English MultiNet model.
  char * mn_name = esp_srmodel_filter(s_models, ESP_MN_PREFIX, ESP_MN_ENGLISH);

  if (mn_name == NULL) {
    ESP_LOGE(TAG, "English MultiNet model not found");
    return ESP_ERR_NOT_FOUND;
  }

  ESP_LOGI(TAG, "MultiNet model: %s", mn_name);

  const esp_mn_iface_t * mn = esp_mn_handle_from_name(mn_name);

  if (mn == NULL) {
    ESP_LOGE(TAG, "MultiNet interface unavailable");
    return ESP_FAIL;
  }

  // 6000 ms recognition timeout.
  model_iface_data_t * data = mn->create(mn_name, 6000);

  if (data == NULL) {
    ESP_LOGE(TAG, "MultiNet model creation failed");
    return ESP_ERR_NO_MEM;
  }

  // Initialize the custom command list.
  ESP_ERROR_CHECK(esp_mn_commands_alloc(mn, data));
  ESP_ERROR_CHECK(esp_mn_commands_clear());

  // MultiNet6 English command phrases.
  ESP_ERROR_CHECK(esp_mn_commands_add(1, "BE HAPPY"));
  ESP_ERROR_CHECK(esp_mn_commands_add(2, "BE ANGRY"));
  ESP_ERROR_CHECK(esp_mn_commands_add(3, "GO TO SLEEP"));
  ESP_ERROR_CHECK(esp_mn_commands_add(4, "WAKE UP"));

  // Apply command changes to the recognition model.
  esp_mn_error_t * cmd_error = esp_mn_commands_update();

  if (cmd_error != NULL) {
    ESP_LOGE(TAG, "Failed to apply speech commands");
    return ESP_FAIL;
  }

  mn->print_active_speech_commands(data);

  *out_mn = mn;
  *out_data = data;

  ESP_LOGI(TAG, "MultiNet initialized successfully");
  return ESP_OK;
}

static void execute_voice_command(int command_id) {
  aruka_event_type_t event;

  switch (command_id) {
    case 1:
      ESP_LOGI(TAG, "Command: BE HAPPY");
      event = ARUKA_EVENT_CMD_HAPPY;
      break;
    case 2:
      ESP_LOGI(TAG, "Command: BE ANGRY");
      event = ARUKA_EVENT_CMD_ANGRY;
      break;
    case 3:
      ESP_LOGI(TAG, "Command: GO TO SLEEP");
      event = ARUKA_EVENT_CMD_SLEEP;
      break;
    case 4:
      ESP_LOGI(TAG, "Command: WAKE UP");
      event = ARUKA_EVENT_CMD_WAKE_UP;
      break;
    default:
      ESP_LOGW(TAG, "Unknown command ID: %d", command_id);
      return;
  }

  esp_err_t ret = aruka_behavior_post_event(event);

  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "Failed to post command event: %s", esp_err_to_name(ret));
  }
}

static void wake_detect_task(void * arg) {
  const esp_mn_iface_t * mn = NULL;
  model_iface_data_t * mn_data = NULL;

  if (init_multinet(&mn, &mn_data) != ESP_OK) {
    ESP_LOGE(TAG, "MultiNet initialization failed");
    vTaskDelete(NULL);
    return;
  }

  const int afe_chunk = s_afe->get_fetch_chunksize(s_afe_data);

  const int mn_chunk = mn->get_samp_chunksize(mn_data);

  if (afe_chunk != mn_chunk) {
    ESP_LOGE(TAG, "Chunk mismatch: AFE=%d MultiNet=%d", afe_chunk, mn_chunk);
    mn->destroy(mn_data);
    vTaskDelete(NULL);
    return;
  }

  bool listening = false;

  ESP_LOGI(TAG, "Voice recognition ready");
  ESP_LOGI(TAG, "Say 'Hi ESP'");

  while (1) {
    afe_fetch_result_t * result = s_afe->fetch(s_afe_data);

    if (!result || result->ret_value == ESP_FAIL) {
      ESP_LOGE(TAG, "AFE fetch failed");
      break;
    }

    // WakeNet: enter the listening state.
    if (result->wakeup_state == WAKENET_DETECTED) {
      ESP_LOGI(TAG, "WAKE WORD DETECTED!");

      mn->clean(mn_data);
      listening = true;

      esp_err_t ret = aruka_behavior_post_event(ARUKA_EVENT_WAKE_WORD);

      if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to post wake event: %s", esp_err_to_name(ret));
      }

      ESP_LOGI(TAG, "Waiting for voice command...");
    }

    // Ignore speech commands before wake-up.
    if (!listening) {
      continue;
    }

    // Process audio using MultiNet.
    esp_mn_state_t state = mn->detect(mn_data, result->data);

    if (state == ESP_MN_STATE_DETECTED) {
      esp_mn_results_t * cmd = mn->get_results(mn_data);

      if (cmd != NULL && cmd->num > 0) {
        int command_id = cmd->command_id[0];

        ESP_LOGI(TAG,
                 "COMMAND DETECTED: id=%d, phrase=%d, confidence=%.3f",
                 command_id,
                 cmd->phrase_id[0],
                 (double)cmd->prob[0]);

        execute_voice_command(command_id);
      }

      // Single-command mode:
      // require another wake word for next command.
      listening = false;

      s_afe->enable_wakenet(s_afe_data);

      ESP_LOGI(TAG, "Waiting for wake word...");
    }

    else if (state == ESP_MN_STATE_TIMEOUT) {
      ESP_LOGI(TAG, "Command listening timed out");

      listening = false;
      s_afe->enable_wakenet(s_afe_data);
      esp_err_t ret = aruka_behavior_post_event(ARUKA_EVENT_COMMAND_TIMEOUT);

      if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to post timeout event: %s", esp_err_to_name(ret));
      }
      ESP_LOGI(TAG, "Waiting for wake word...");
    }
  }

  mn->destroy(mn_data);
  vTaskDelete(NULL);
}

esp_err_t aruka_voice_init(void) {
  ESP_LOGI(TAG, "Initializing ESP-SR WakeNet...");

  // Initialize the onboard microphone.
  s_mic = bsp_audio_codec_microphone_init();

  if (!s_mic) {
    ESP_LOGE(TAG, "Microphone init failed");
    return ESP_FAIL;
  }

  esp_codec_dev_sample_info_t format = {
      .sample_rate = SAMPLE_RATE,
      .channel = 1,
      .bits_per_sample = 16,
  };

  if (esp_codec_dev_open(s_mic, &format) != 0) {
    ESP_LOGE(TAG, "Cannot open microphone");
    return ESP_FAIL;
  }

  // The model partition is named "model".
  s_models = esp_srmodel_init("model");

  if (!s_models || s_models->num == 0) {
    ESP_LOGE(TAG, "No ESP-SR models found");
    return ESP_FAIL;
  }

  ESP_LOGI(TAG, "Found %d speech models", s_models->num);

  // One microphone channel: "M".
  afe_config_t * config = afe_config_init("M", s_models, AFE_TYPE_SR, AFE_MODE_LOW_COST);

  if (!config) {
    ESP_LOGE(TAG, "AFE config failed");
    return ESP_FAIL;
  }

  config->wakenet_init = true;

  if (!config->wakenet_model_name) {
    ESP_LOGE(TAG, "No WakeNet model selected");
    afe_config_free(config);
    return ESP_FAIL;
  }

  ESP_LOGI(TAG, "WakeNet model: %s", config->wakenet_model_name);

  s_afe = esp_afe_handle_from_config(config);

  if (!s_afe) {
    ESP_LOGE(TAG, "AFE interface init failed");
    afe_config_free(config);
    return ESP_FAIL;
  }

  s_afe_data = s_afe->create_from_config(config);
  afe_config_free(config);

  if (!s_afe_data) {
    ESP_LOGE(TAG, "AFE instance creation failed");
    return ESP_FAIL;
  }

  BaseType_t feed_ok = xTaskCreatePinnedToCore(
      audio_feed_task, "aruka_audio_feed", 8192, NULL, 5, NULL, 0);

  if (feed_ok != pdPASS) {
    ESP_LOGE(TAG, "Cannot create feed task");
    return ESP_ERR_NO_MEM;
  }

  BaseType_t detect_ok = xTaskCreatePinnedToCore(
      wake_detect_task, "aruka_wake_detect", 8192, NULL, 5, NULL, 1);

  if (detect_ok != pdPASS) {
    ESP_LOGE(TAG, "Cannot create detection task");
    return ESP_ERR_NO_MEM;
  }

  ESP_LOGI(TAG, "Aruka voice system initialized");

  return ESP_OK;
}
