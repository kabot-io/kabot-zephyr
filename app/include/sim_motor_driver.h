#pragma once

#include "motor_driver.h"

/**
 * @brief Simulation motor driver.
 *
 * On @c native_sim this driver acts as a stub that logs effort values.
 * It is intentionally minimal so that it can be extended into a full
 * Gazebo / ROS 2 bridge in a future revision — for example by publishing
 * the effort value to a Gazebo topic over a shared-memory transport.
 */
struct sim_motor_driver {
    struct motor_driver base; /**< Must be first — enables casting to motor_driver. */
    const char *name;         /**< Motor name used in log output and future sim topics. */
};

extern const struct motor_driver_api sim_motor_driver_api;

#define SIM_MOTOR_DRIVER_DEFINE(motor_name)       \
    {                                             \
        .base = {.api = &sim_motor_driver_api},   \
        .name = motor_name,                       \
    }
