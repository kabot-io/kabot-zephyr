#pragma once

#include "motor_driver.h"

#include <stdint.h>

#include <zephyr/devicetree.h>
#include <zephyr/drivers/pwm.h>

extern const struct motor_driver_api esc_driver_api;

struct esc_driver {
    struct motor_driver base; /**< Must be first — enables casting to motor_driver. */
    struct pwm_dt_spec pwm;
    uint32_t reverse_pulse;
    uint32_t stop_pulse;
    uint32_t forward_pulse;
};

#define ESC_DRIVER_FROM_DT(node_id)                                                                \
    {                                                                                              \
        .base = {.api = &esc_driver_api},                                                          \
        .pwm = PWM_DT_SPEC_GET(node_id),                                                           \
        .reverse_pulse = DT_PROP(node_id, reverse_pulse),                                          \
        .stop_pulse = DT_PROP(node_id, stop_pulse),                                                \
        .forward_pulse = DT_PROP(node_id, forward_pulse),                                          \
    }
