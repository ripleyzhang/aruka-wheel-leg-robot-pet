
#include "expression.h"

#include <stdbool.h>
#include <stdint.h>

#include "bsp/esp-box-3.h"
#include "esp_log.h"
#include "lvgl.h"

static const char * TAG = "ARUKA_EXPR";

// LVGL objects: created only once.
static lv_obj_t * s_face = NULL;
static lv_obj_t * s_features = NULL;
static lv_obj_t * s_left_eye = NULL;
static lv_obj_t * s_right_eye = NULL;
static lv_obj_t * s_mouth = NULL;
static lv_obj_t * s_label = NULL;

// Current visual configuration.
static aruka_face_persona_t s_persona = ARUKA_FACE_PERSONA_ALLUKA;

static aruka_face_expression_t s_mode = ARUKA_FACE_DEFAULT;
// Retain the display handle if initialization fails after BSP startup, so a
// retry cannot initialize the LCD/LVGL driver twice.
static lv_display_t * s_display = NULL;
static bool s_initialized = false;

static void setup_object(lv_obj_t * obj) {
  lv_obj_set_style_border_width(obj, 0, 0);
  lv_obj_set_style_shadow_width(obj, 0, 0);
  lv_obj_set_style_pad_all(obj, 0, 0);

  lv_obj_set_scrollable(obj, false);
}

static void draw_oval(lv_obj_t * obj,
                      int width,
                      int height,
                      int x,
                      int y,
                      lv_color_t color) {
  lv_obj_set_size(obj, width, height);
  lv_obj_align(obj, LV_ALIGN_CENTER, x, y);

  lv_obj_set_style_bg_color(obj, color, 0);
  lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, 0);

  lv_obj_set_style_radius(obj, LV_RADIUS_CIRCLE, 0);

  lv_obj_set_style_border_width(obj, 0, 0);
}

// This callback runs inside LVGL's animation system.
static void face_fade_cb(void * obj, int32_t value) {
  lv_obj_set_style_opa((lv_obj_t *)obj, (lv_opa_t)value, 0);
}

static void animate_face_appearance(void) {
  // Cancel any previous animation on this object.
  lv_anim_delete(s_features, face_fade_cb);

  lv_obj_set_style_opa(s_features, LV_OPA_TRANSP, 0);

  lv_anim_t anim;
  lv_anim_init(&anim);

  lv_anim_set_var(&anim, s_features);
  lv_anim_set_exec_cb(&anim, face_fade_cb);

  lv_anim_set_values(&anim, LV_OPA_TRANSP, LV_OPA_COVER);

  lv_anim_set_duration(&anim, 450);
  if (lv_anim_start(&anim) == NULL) {
    // Allocation failure must never leave a permanently invisible face.
    lv_obj_set_style_opa(s_features, LV_OPA_COVER, 0);
  }
}

static void apply_face(aruka_face_persona_t persona, aruka_face_expression_t mode) {
  bool nanika = (persona == ARUKA_FACE_PERSONA_NANIKA);

  lv_color_t background = nanika ? lv_color_hex(0x101018) : lv_color_hex(0xF2EDF8);

  lv_color_t foreground = nanika ? lv_color_hex(0xF5F1FC) : lv_color_hex(0x7966A1);

  // Background.
  lv_obj_set_style_bg_color(s_face, background, 0);

  lv_obj_set_style_bg_opa(s_face, LV_OPA_COVER, 0);

  // Persona name.
  lv_label_set_text_static(s_label, nanika ? "NANIKA" : "ALLUKA");

  lv_obj_set_style_text_color(s_label, foreground, 0);

  // Eye sizes.
  int eye_width = nanika ? 27 : 23;
  int eye_height = nanika ? 36 : 31;

  // Listening is a temporary visual mode.
  if (mode == ARUKA_FACE_LISTENING) {
    eye_width += 4;
    eye_height += 5;
  }

  draw_oval(s_left_eye, eye_width, eye_height, -46, -12, foreground);

  draw_oval(s_right_eye, eye_width, eye_height, 46, -12, foreground);

  // Default mouth.
  int mouth_width = nanika ? 18 : 35;
  int mouth_height = nanika ? 25 : 9;

  switch (mode) {
    case ARUKA_FACE_DEFAULT:
      break;

    case ARUKA_FACE_LISTENING:
      mouth_width = 15;
      mouth_height = 16;
      break;

    case ARUKA_FACE_WISH_READY:
      mouth_width = 18;
      mouth_height = 25;
      break;

    case ARUKA_FACE_WISH_GRANTED:
      mouth_width = 34;
      mouth_height = 14;
      break;

    default:
      break;
  }

  draw_oval(s_mouth, mouth_width, mouth_height, 0, 46, foreground);
}

esp_err_t aruka_set_face(aruka_face_persona_t persona,
                         aruka_face_expression_t expression) {
  if (!s_initialized) {
    return ESP_ERR_INVALID_STATE;
  }

  if (persona != ARUKA_FACE_PERSONA_ALLUKA && persona != ARUKA_FACE_PERSONA_NANIKA) {
    return ESP_ERR_INVALID_ARG;
  }

  if (expression < ARUKA_FACE_DEFAULT || expression > ARUKA_FACE_WISH_GRANTED) {
    return ESP_ERR_INVALID_ARG;
  }

  // Wish-specific modes belong to Nanika.
  if ((expression == ARUKA_FACE_WISH_READY || expression == ARUKA_FACE_WISH_GRANTED) &&
      persona != ARUKA_FACE_PERSONA_NANIKA) {
    return ESP_ERR_INVALID_ARG;
  }

  if (!bsp_display_lock(1000)) {
    return ESP_ERR_TIMEOUT;
  }

  // An identical refresh should let an in-flight transition finish.
  if (persona == s_persona && expression == s_mode) {
    bsp_display_unlock();
    return ESP_OK;
  }

  bool persona_changed = (persona != s_persona);

  // Remove an old animation before redrawing.
  lv_anim_delete(s_features, face_fade_cb);

  lv_obj_set_style_opa(s_features, LV_OPA_COVER, 0);

  apply_face(persona, expression);

  if (persona_changed) {
    animate_face_appearance();
  }

  s_persona = persona;
  s_mode = expression;

  bsp_display_unlock();

  ESP_LOGI(TAG,
           "Persona=%s, Face=%d",
           persona == ARUKA_FACE_PERSONA_NANIKA ? "NANIKA" : "ALLUKA",
           expression);

  return ESP_OK;
}

esp_err_t aruka_expression_init(void) {
  if (s_face != NULL) {
    return ESP_ERR_INVALID_STATE;
  }

  ESP_LOGI(TAG, "Initializing LVGL display...");

  // Preserve the project's existing BSP configuration and rotation defaults.
  if (s_display == NULL) {
    s_display = bsp_display_start();
  }

  if (s_display == NULL) {
    ESP_LOGE(TAG, "Display initialization failed");
    return ESP_FAIL;
  }

  if (!bsp_display_lock(1000)) {
    return ESP_ERR_TIMEOUT;
  }

  lv_obj_t * screen = lv_screen_active();

  // Full-screen face container.
  s_face = lv_obj_create(screen);

  setup_object(s_face);

  lv_obj_set_size(s_face,
                  lv_display_get_horizontal_resolution(s_display),
                  lv_display_get_vertical_resolution(s_display));

  lv_obj_center(s_face);

  lv_obj_set_style_radius(s_face, 0, 0);

  // Container for eyes, mouth, and text.
  s_features = lv_obj_create(s_face);
  setup_object(s_features);

  lv_obj_set_size(s_features, lv_pct(100), lv_pct(100));

  lv_obj_center(s_features);

  lv_obj_set_style_bg_opa(s_features, LV_OPA_TRANSP, 0);

  // Eyes.
  s_left_eye = lv_obj_create(s_features);
  s_right_eye = lv_obj_create(s_features);

  setup_object(s_left_eye);
  setup_object(s_right_eye);

  // Mouth.
  s_mouth = lv_obj_create(s_features);
  setup_object(s_mouth);

  // Persona label.
  s_label = lv_label_create(s_features);

  lv_obj_align(s_label, LV_ALIGN_TOP_MID, 0, 18);

  // First frame: Alluka.
  apply_face(ARUKA_FACE_PERSONA_ALLUKA, ARUKA_FACE_DEFAULT);

  s_persona = ARUKA_FACE_PERSONA_ALLUKA;
  s_mode = ARUKA_FACE_DEFAULT;
  s_initialized = true;

  bsp_display_unlock();

  // Turn on LCD backlight.
  esp_err_t ret = bsp_display_backlight_on();

  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "Cannot enable backlight: %s", esp_err_to_name(ret));
    return ret;
  }

  ESP_LOGI(TAG, "Alluka face initialized");

  return ESP_OK;
}
