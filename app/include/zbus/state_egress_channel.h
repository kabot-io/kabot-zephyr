#pragma once

#include "protos/state_control_msg.pb.h"

#include <zephyr/zbus/zbus.h>

int publish_state_egress_msg(const State *msg, k_timeout_t timeout);
bool state_egress_channel_validator(const void *msg, size_t msg_size);

ZBUS_CHAN_DECLARE(state_egress_channel);
