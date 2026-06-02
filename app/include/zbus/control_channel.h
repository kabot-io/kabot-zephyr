#pragma once

#include "protos/state_control_msg.pb.h"

#include <zephyr/zbus/zbus.h>

int publish_control_msg(const Control *msg, k_timeout_t timeout);
bool control_channel_validator(const void *msg, size_t msg_size);

ZBUS_CHAN_DECLARE(control_channel);
