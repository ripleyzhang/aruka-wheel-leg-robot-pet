#include "expression.h"
#include "voice.h"

#include <stdint.h>
#include <stdlib.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_log.h"
#include "esp_codec_dev.h"
#include "bsp/esp-box-3.h"

#include "esp_afe_sr_iface.h"
#include "esp_afe_sr_models.h"
#include "esp_afe_config.h"
#include "model_path.h"

#define SAMPLE_RATE 16000

static const char *TAG = "ARUKA_VOICE";

static esp_codec_dev_handle_t s_mic = NULL;
static const esp_afe_sr_iface_t *s_afe = NULL;
static esp_afe_sr_data_t *s_afe_data = NULL;

// Task 1: continuously capture microphone data.
static void audio_feed_task(void *arg)
{
    const int chunk = s_afe->get_feed_chunksize(s_afe_data);
    const int channels = s_afe->get_feed_channel_num(s_afe_data);

    const size_t bytes =
        (size_t)chunk * channels * sizeof(int16_t);

    int16_t *buffer = malloc(bytes);

    if (!buffer) {
        ESP_LOGE(TAG, "Cannot allocate feed buffer");
        vTaskDelete(NULL);
        return;
    }

    ESP_LOGI(TAG, "Audio feed started: %d samples, %d channels",
             chunk, channels);

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

// Task 2: continuously check WakeNet results.
static void wake_detect_task(void *arg)
{
    ESP_LOGI(TAG, "WakeNet detection started");
    ESP_LOGI(TAG, "Say the configured wake word!");

    while (1) {
        afe_fetch_result_t *result = s_afe->fetch(s_afe_data);

        if (!result || result->ret_value == ESP_FAIL) {
            ESP_LOGE(TAG, "AFE fetch failed");
            break;
        }

        if (result->wakeup_state == WAKENET_DETECTED) {
            ESP_LOGI(TAG, "==========================");
            ESP_LOGI(TAG, "WAKE WORD DETECTED!");
            ESP_LOGI(TAG, "Model index: %d",
                     result->wakenet_model_index);
            ESP_LOGI(TAG, "Word index: %d",
                     result->wake_word_index);

            ESP_LOGI(TAG, "Switching face to LISTENING...");
            esp_err_t ret = aruka_set_expression(ARUKA_EXPR_LISTENING);

            if (ret == ESP_OK) {
                ESP_LOGI(TAG, "Expression switched successfully!");
            } else {
                ESP_LOGI(TAG, "Expression switched failed: %s",
                        esp_err_to_name(ret));
            }
            ESP_LOGI(TAG, "==========================");
        }
    }

    vTaskDelete(NULL);
}

esp_err_t aruka_voice_init(void)
{
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
    srmodel_list_t *models = esp_srmodel_init("model");

    if (!models || models->num == 0) {
        ESP_LOGE(TAG, "No ESP-SR models found");
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "Found %d speech models", models->num);

    // One microphone channel: "M".
    afe_config_t *config = afe_config_init(
        "M",
        models,
        AFE_TYPE_SR,
        AFE_MODE_LOW_COST
    );

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

    ESP_LOGI(TAG, "WakeNet model: %s",
             config->wakenet_model_name);

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
        audio_feed_task,
        "aruka_audio_feed",
        8192,
        NULL,
        5,
        NULL,
        0
    );

    if (feed_ok != pdPASS) {
        ESP_LOGE(TAG, "Cannot create feed task");
        return ESP_ERR_NO_MEM;
    }

    BaseType_t detect_ok = xTaskCreatePinnedToCore(
        wake_detect_task,
        "aruka_wake_detect",
        8192,
        NULL,
        5,
        NULL,
        1
    );

    if (detect_ok != pdPASS) {
        ESP_LOGE(TAG, "Cannot create detection task");
        return ESP_ERR_NO_MEM;
    }

    ESP_LOGI(TAG, "Aruka voice system initialized");

    return ESP_OK;
}

