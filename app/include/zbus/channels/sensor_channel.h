#pragma once

#include <zephyr/zbus/zbus.h>
#include "zbus/sensor_msg.h"

/**
 * @brief Publishes a sensor_msg to the sensor_channel.
 *
 * @param msg Pointer to the sensor_msg structure to publish.
 * @param timeout Timeout for publishing the message.
 * @return 0 if the message was published successfully, negative errno on failure.
 */
int publish_sensor_msg(const struct sensor_msg *msg, k_timeout_t timeout);

/**
 * @brief Validates a sensor_msg for the sensor_channel.
 *
 * @param msg Pointer to the message to validate.
 * @param msg_size Size of the message.
 * @return true if the message is valid, false otherwise.
 */
bool sensor_channel_validator(const void *msg, size_t msg_size);

ZBUS_CHAN_DECLARE(sensor_channel);
