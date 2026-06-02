#pragma once

#include "protos/state_control_msg.pb.h"

#include <zephyr/zbus/zbus.h>

/**
 * @brief Publish a Control message to the control channel.
 *
 * @param msg Pointer to the Control message to publish.
 * @param timeout Timeout for the publish operation.
 * @return 0 on success, negative errno on failure.
 */
int publish_control_msg(const Control *msg, k_timeout_t timeout);

/**
 * @brief Validator function for the control channel.
 *
 * @param msg Pointer to the message to validate.
 * @param msg_size Size of the message.
 * @return true if the message is valid, false otherwise.
 */
bool control_channel_validator(const void *msg, size_t msg_size);

ZBUS_CHAN_DECLARE(control_channel);
