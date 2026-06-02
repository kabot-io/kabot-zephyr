#include "zbus/channels/state_egress_channel.h"

#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(state_egress_channel, LOG_LEVEL_DBG);

int publish_state_egress_msg(const State *msg, k_timeout_t timeout)
{
    return zbus_chan_pub(&state_egress_channel, msg, timeout);
}

bool state_egress_channel_validator(const void *msg, size_t msg_size)
{
    return (msg != NULL) && (msg_size == sizeof(State));
}

ZBUS_CHAN_DEFINE(state_egress_channel,
                 State,
                 state_egress_channel_validator,
                 NULL,
                 ZBUS_OBSERVERS(state_udp_sender),
                 ZBUS_MSG_INIT(0));
