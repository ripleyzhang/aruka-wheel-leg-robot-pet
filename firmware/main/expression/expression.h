
#pragma once

#include "esp_err.h"

// Aruka's facial expressions
typedef enum {
    ARUKA_EXPR_IDLE = 0,
    ARUKA_EXPR_HAPPY,
    ARUKA_EXPR_ANGRY,
    ARUKA_EXPR_SLEEPY,
    ARUKA_EXPR_LISTENING,
    ARUKA_EXPR_COUNT
} aruka_expression_t;

// Initialize LCD and LVGL
esp_err_t aruka_expression_init(void);

// Change the current facial expression
esp_err_t aruka_set_expression(aruka_expression_t expression);

// Get the current expression
aruka_expression_t  aruka_get_expression(void);

