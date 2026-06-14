#pragma once

#include <zephyr/drivers/sensor_data_types.h>

/**
 * @brief Message type for sensor data.
 *
 * This message contains two encoder readings.
 * Each encoder reading is a full sensor_q31_data payload with its own timestamp.
 */
struct sensor_msg {
	/** @brief Left encoder reading. */
	struct sensor_q31_data left_encoder;

	/** @brief Right encoder reading. */
	struct sensor_q31_data right_encoder;
};
