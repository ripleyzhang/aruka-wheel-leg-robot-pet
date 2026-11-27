
#pragma once

#include "esp_err.h"

// Visual persona.
// This describes appearance, not Wish Cycle state.
typedef enum {
  ARUKA_FACE_PERSONA_ALLUKA = 0,
  ARUKA_FACE_PERSONA_NANIKA
} aruka_face_persona_t;

// Visual modes implemented in EXP-03.
typedef enum {
  ARUKA_FACE_DEFAULT = 0,
  ARUKA_FACE_LISTENING,
  ARUKA_FACE_WISH_READY,
  ARUKA_FACE_WISH_GRANTED
} aruka_face_expression_t;

// Initialize LCD, LVGL and face objects.
esp_err_t aruka_expression_init(void);

// Set a face after initialization. Wish modes are valid only for Nanika.
// Returns INVALID_ARG for unsupported combinations, INVALID_STATE before init,
// or TIMEOUT if the BSP display lock cannot be acquired.
esp_err_t aruka_set_face(aruka_face_persona_t persona,
                         aruka_face_expression_t expression);

// The EXP-02 emotion setter/getter were removed: reward faces cannot be
// represented accurately by the old emotion enum. Behavior owns face selection.
