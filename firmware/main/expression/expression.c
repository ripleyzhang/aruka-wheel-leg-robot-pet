
#include "expression.h"

#include "bsp/esp-box-3.h"
#include "esp_log.h"
#include "lvgl.h"

static const char * TAG = "ARUKA_FACE";

static lv_display_t * s_display = NULL;
static lv_obj_t * s_screen = NULL;

static aruka_expression_t s_current = ARUKA_EXPR_COUNT;

#define BG_COLOR 0x171724
#define PURPLE 0xB8A0FF
#define PINK 0xFF91AB
#define BLUE 0x9AA9D6
#define CYAN 0xA9E6F4

// Create a simple filled shape at an absolute position.
static lv_obj_t *
make_shape(lv_obj_t * parent, int x, int y, int w, int h, uint32_t color, int radius) {
  lv_obj_t * obj = lv_obj_create(parent);

  lv_obj_set_pos(obj, x, y);
  lv_obj_set_size(obj, w, h);

  lv_obj_set_style_bg_color(obj, lv_color_hex(color), 0);
  lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, 0);
  lv_obj_set_style_radius(obj, radius, 0);

  lv_obj_set_style_border_width(obj, 0, 0);
  lv_obj_set_style_pad_all(obj, 0, 0);
  lv_obj_set_style_shadow_width(obj, 0, 0);

  lv_obj_remove_flag(obj, LV_OBJ_FLAG_SCROLLABLE);

  return obj;
}

// Draw a solid circle.
static void draw_circle(int x, int y, int diameter, uint32_t color) {
  make_shape(s_screen, x, y, diameter, diameter, color, LV_RADIUS_CIRCLE);
}

// Draw a horizontal rounded rectangle.
static void draw_bar(int x, int y, int w, int h, uint32_t color) {
  make_shape(s_screen, x, y, w, h, color, LV_RADIUS_CIRCLE);
}

// Draw a simple smiling mouth.
static void draw_smile(uint32_t color) {
  // Three segments forming a U-shaped mouth.
  draw_bar(139, 149, 8, 17, color);
  draw_bar(173, 149, 8, 17, color);
  draw_bar(145, 160, 30, 8, color);
}

// Draw a simple frowning mouth.
static void draw_frown(uint32_t color) {
  draw_bar(141, 159, 8, 14, color);
  draw_bar(171, 159, 8, 14, color);
  draw_bar(147, 154, 26, 8, color);
}

// Add a text label to the screen.
static void draw_label(const char * text, int y, uint32_t color) {
  lv_obj_t * label = lv_label_create(s_screen);

  lv_label_set_text(label, text);

  lv_obj_set_style_text_color(label, lv_color_hex(color), 0);

  lv_obj_set_style_text_font(label, &lv_font_montserrat_14, 0);

  lv_obj_align(label, LV_ALIGN_TOP_MID, 0, y);
}

// Draw one complete expression.
static void draw_expression(aruka_expression_t expr) {
  // Remove old facial objects before redrawing.
  lv_obj_clean(s_screen);

  lv_obj_set_style_bg_color(s_screen, lv_color_hex(BG_COLOR), 0);
  lv_obj_set_style_bg_opa(s_screen, LV_OPA_COVER, 0);

  lv_obj_remove_flag(s_screen, LV_OBJ_FLAG_SCROLLABLE);

  draw_label("ARUKA", 14, PURPLE);

  switch (expr) {
    case ARUKA_EXPR_IDLE:
      // Neutral round eyes.
      draw_circle(99, 88, 28, PURPLE);
      draw_circle(193, 88, 28, PURPLE);

      draw_bar(145, 157, 30, 6, PURPLE);
      draw_label("IDLE", 208, PURPLE);
      break;

    case ARUKA_EXPR_HAPPY:
      // Closed, smiling eyes.
      draw_bar(94, 103, 42, 9, PURPLE);
      draw_bar(184, 103, 42, 9, PURPLE);

      draw_smile(PURPLE);
      draw_label("HAPPY", 208, PURPLE);
      break;

    case ARUKA_EXPR_ANGRY:
      // Eyebrows and narrowed eyes.
      draw_bar(91, 81, 45, 8, PINK);
      draw_bar(184, 81, 45, 8, PINK);

      draw_bar(101, 104, 25, 12, PINK);
      draw_bar(194, 104, 25, 12, PINK);

      draw_frown(PINK);
      draw_label("ANGRY", 208, PINK);
      break;

    case ARUKA_EXPR_SLEEPY:
      // Closed eyes.
      draw_bar(93, 107, 43, 5, BLUE);
      draw_bar(184, 107, 43, 5, BLUE);

      draw_bar(154, 154, 12, 5, BLUE);
      draw_label("SLEEPY", 208, BLUE);
      break;

    case ARUKA_EXPR_LISTENING:
      // Large eyes with dark pupils.
      draw_circle(88, 81, 44, CYAN);
      draw_circle(188, 81, 44, CYAN);

      draw_circle(103, 95, 15, BG_COLOR);
      draw_circle(203, 95, 15, BG_COLOR);

      // Small open mouth: outer colored circle,
      // inner background circle.
      draw_circle(150, 151, 20, CYAN);
      draw_circle(155, 156, 10, BG_COLOR);

      draw_label("LISTENING", 208, CYAN);
      break;

    default:
      draw_label("UNKNOWN", 105, PINK);
      break;
  }
}

esp_err_t aruka_expression_init(void) {
  ESP_LOGI(TAG, "Initializing LVGL display...");

  if (s_display != NULL) {
    return ESP_ERR_INVALID_STATE;
  }
  s_display = bsp_display_start();

  if (s_display == NULL) {
    ESP_LOGE(TAG, "Display initialization failed");
    return ESP_FAIL;
  }

  // LVGL is not thread-safe. Lock before creating UI.
  if (!bsp_display_lock(1000)) {
    ESP_LOGE(TAG, "LVGL lock failed");
    return ESP_ERR_TIMEOUT;
  }

  s_screen = lv_display_get_screen_active(s_display);

  draw_expression(ARUKA_EXPR_IDLE);
  s_current = ARUKA_EXPR_IDLE;

  // Release LVGL lock.
  bsp_display_unlock();

  // Enable LCD backlight.
  esp_err_t ret = bsp_display_backlight_on();

  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "Backlight failed");
    return ret;
  }

  ESP_LOGI(TAG, "Expression system ready");

  return ESP_OK;
}

esp_err_t aruka_set_expression(aruka_expression_t expression) {
  if (expression < ARUKA_EXPR_IDLE || expression >= ARUKA_EXPR_COUNT) {
    return ESP_ERR_INVALID_ARG;
  }

  if (s_screen == NULL) {
    return ESP_ERR_INVALID_STATE;
  }

  if (!bsp_display_lock(1000)) {
    return ESP_ERR_TIMEOUT;
  }

  if (expression != s_current) {
    draw_expression(expression);
    s_current = expression;
  }

  bsp_display_unlock();

  ESP_LOGI(TAG, "Expression: %d", (int)expression);

  return ESP_OK;
}

aruka_expression_t aruka_get_expression(void) {
  return s_current;
}
