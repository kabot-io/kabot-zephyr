#include "motor/motor_math.h"
#include "zbus/effort_channel.h"

#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(effort_channel, LOG_LEVEL_DBG);

int publish_effort_msg(const EffortMsg *msg, k_timeout_t timeout)
{
    LOG_INF("Publishing effort message: left=%.3f, right=%.3f", msg->left, msg->right);
    return zbus_chan_pub(&effort_channel, msg, timeout);
}

bool effort_channel_validator(const void *msg, size_t msg_size)
{
    const EffortMsg *effort = msg;

    bool valid = (msg_size == sizeof(EffortMsg)) && motor_effort_is_valid(effort->left)
                 && motor_effort_is_valid(effort->right);

    if (!valid) {
        LOG_WRN("Effort channel validator failed: left=%.3f, right=%.3f, msg_size=%zu",
                effort->left, effort->right, msg_size);
    }

    return valid;
}

ZBUS_CHAN_DEFINE(effort_channel,
                 EffortMsg,
                 effort_channel_validator,
                 NULL,
                 ZBUS_OBSERVERS(effort_subscriber),
                 ZBUS_MSG_INIT(.left = 0.0f, .right = 0.0f));
