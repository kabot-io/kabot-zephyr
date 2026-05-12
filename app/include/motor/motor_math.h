#pragma once

#include <errno.h>
#include <limits.h>
#include <stdbool.h>
#include <stdint.h>

#include <zephyr/sys/util.h>

#define MOTOR_EFFORT_Q31_MIN INT32_MIN
#define MOTOR_EFFORT_Q31_MAX INT32_MAX

#define MOTOR_EFFORT_PERCENT_MIN -100
#define MOTOR_EFFORT_PERCENT_MAX 100

static inline bool motor_effort_q31_is_valid(int32_t effort_q31)
{
    ARG_UNUSED(effort_q31);
    return true;
}

static inline int motor_percent_to_q31(int32_t effort_percent, int32_t *effort_q31)
{
    if (effort_q31 == NULL) {
        return -EINVAL;
    }

    if (effort_percent < MOTOR_EFFORT_PERCENT_MIN || effort_percent > MOTOR_EFFORT_PERCENT_MAX) {
        return -EINVAL;
    }

    if (effort_percent == MOTOR_EFFORT_PERCENT_MIN) {
        *effort_q31 = MOTOR_EFFORT_Q31_MIN;
        return 0;
    }

    if (effort_percent == MOTOR_EFFORT_PERCENT_MAX) {
        *effort_q31 = MOTOR_EFFORT_Q31_MAX;
        return 0;
    }

    *effort_q31 = (int32_t)(((int64_t)effort_percent * (int64_t)MOTOR_EFFORT_Q31_MAX) / 100);
    return 0;
}

static inline int32_t motor_q31_to_percent(int32_t effort_q31)
{
    if (effort_q31 <= MOTOR_EFFORT_Q31_MIN) {
        return MOTOR_EFFORT_PERCENT_MIN;
    }

    if (effort_q31 >= MOTOR_EFFORT_Q31_MAX) {
        return MOTOR_EFFORT_PERCENT_MAX;
    }

    int64_t scaled = ((int64_t)effort_q31 * 100);

    if (scaled >= 0) {
        scaled += (MOTOR_EFFORT_Q31_MAX / 2);
    } else {
        scaled -= (MOTOR_EFFORT_Q31_MAX / 2);
    }

    return (int32_t)(scaled / MOTOR_EFFORT_Q31_MAX);
}
