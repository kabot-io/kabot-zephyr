#pragma once

#include <zephyr/zbus/zbus.h>
#include "zbus/effort_msg.h"

#define EFFORT_MSG_INVALID {.left = -127, .right = -127}

/**
 * @brief Publishes an effort_msg to the effort_channel.
 *
 * @param msg Pointer to the effort_msg structure to publish.
 * @param timeout Timeout for publishing the message.
 * @return true if the message was published successfully, false otherwise.
 */
bool publish_effort_msg(const struct effort_msg *msg, k_timeout_t timeout);

/**
 * @brief Validates an effort_msg for the effort_channel.
 *
 * @param msg Pointer to the message to validate.
 * @param msg_size Size of the message.
 * @return true if the message is valid, false otherwise.
 */
bool effort_channel_validator(const void *msg, size_t msg_size);

ZBUS_CHAN_DECLARE(effort_channel);
