
#include "expression.h"

#include "esp_log.h"
#include "bsp/esp-box-3.h"
#include "lvgl.h"

static const char *TAG = "ARUKA_DISPLAY";

esp_err_t aruka_expression_init(void)
{
    ESP_LOGI(TAG, "Initializing LVGL display...");

    // Initialize LCD, touch input and LVGL task.
    lv_display_t *display = bsp_display_start();

    if (display == NULL) {
        ESP_LOGE(TAG, "Display initialization failed");
        return ESP_FAIL;
    }

    // LVGL is not thread-safe. Lock before creating UI.
    if (!bsp_display_lock(1000)) {
        ESP_LOGE(TAG, "Cannot acquire LVGL lock");
        return ESP_ERR_TIMEOUT;
    }

    // Get the active screen of this display.
    lv_obj_t *screen = lv_display_get_screen_active(display);

    // Dark purple-black background.
    lv_obj_set_style_bg_color(
        screen, lv_color_hex(0x171724), 0);
    lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, 0);

    // Main title.
    lv_obj_t *title = lv_label_create(screen);
    lv_label_set_text(title, "ARUKA");

    lv_obj_set_style_text_color(
        title, lv_color_hex(0xE7DBFF), 0);
    lv_obj_set_style_text_font(
        title, &lv_font_montserrat_14, 0);

    lv_obj_align(
        title, LV_ALIGN_CENTER, 0, -45);

    // Status text.
    lv_obj_t *status = lv_label_create(screen);
    lv_label_set_text(status, "SYSTEM ONLINE");

    lv_obj_set_style_text_color(
        status, lv_color_hex(0xB8A0FF), 0);
    lv_obj_set_style_text_font(
        status, &lv_font_montserrat_14, 0);

    lv_obj_align(
        status, LV_ALIGN_CENTER, 0, 30);

    // Release LVGL lock.
    bsp_display_unlock();

    // Enable LCD backlight.
    esp_err_t ret = bsp_display_backlight_on();

    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Backlight enable failed: %s",
                 esp_err_to_name(ret));
        return ret;
    }

    ESP_LOGI(TAG, "Aruka display initialized!");

    return ESP_OK;
}

