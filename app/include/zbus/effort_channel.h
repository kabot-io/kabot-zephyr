#pragma once

#include "zbus/effort_msg.h"

#include <zephyr/zbus/zbus.h>

int publish_effort_msg(const struct effort_msg *msg, k_timeout_t timeout);
bool effort_channel_validator(const void *msg, size_t msg_size);

ZBUS_CHAN_DECLARE(effort_channel);