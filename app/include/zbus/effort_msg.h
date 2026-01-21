#pragma once

#include <zephyr/types.h>

#define EFFORT_VALUE_MIN -100
#define EFFORT_VALUE_MAX 100

/**
 * @brief Message type for motor effort commands.
 *
 * This message contains
 * left and right effort values for motor control.
 *
 * @note Values are integer percentage of motor maximum effort. Range: [-100, 100].
 */
struct effort_msg {
    /** @brief Effort value for the left motor. */
    int8_t left;
    /** @brief Effort value for the right motor. */
    int8_t right;
};
