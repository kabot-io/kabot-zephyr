#pragma once

#include "motor/motor_math.h"

#include <zephyr/types.h>

#define EFFORT_VALUE_MIN MOTOR_EFFORT_Q31_MIN
#define EFFORT_VALUE_MAX MOTOR_EFFORT_Q31_MAX

/**
 * @brief Message type for motor effort commands.
 *
 * This message contains left and right effort values for motor control.
 *
 * @note Values are normalized Q31 effort. Range: [INT32_MIN, INT32_MAX].
 */
struct effort_msg {
    int32_t left;
    int32_t right;
};
