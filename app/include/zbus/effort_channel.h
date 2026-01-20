#pragma once

#include <zephyr/zbus/zbus.h>
#include "zbus/effort_msg.h"

#define EFFORT_MSG_INVALID {.left = -127, .right = -127};

bool publish_effort_msg(const struct effort_msg *msg, k_timeout_t timeout);
bool effort_channel_validator(const void *msg, size_t msg_size);

ZBUS_CHAN_DECLARE(effort_channel);
