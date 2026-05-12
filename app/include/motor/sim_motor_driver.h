#pragma once

/**
 * @brief Simulation motor driver configuration.
 *
 * On @c native_sim this driver acts as a stub that logs effort values.
 * It is intentionally minimal so that it can be extended into a full
 * Gazebo / ROS 2 bridge in a future revision.
 */
struct sim_motor_config {
    const char *name;
};