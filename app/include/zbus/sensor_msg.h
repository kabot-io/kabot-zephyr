#pragma once

#include <zephyr/drivers/sensor_data_types.h>

/**
 * @brief Message type for sensor data.
 *
 * This message contains reading from a single sensor channel
 *
 * @note Values are sensor_q31_data
 */
struct sensor_msg {
    /** @brief Sensor data. */
    struct sensor_q31_data data;
};
