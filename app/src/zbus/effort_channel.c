#include "motor/motor_math.h"
#include "zbus/effort_channel.h"
#include "zbus/effort_msg.h"

#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(effort_channel, LOG_LEVEL_DBG);

int publish_effort_msg(const struct effort_msg *msg, k_timeout_t timeout)
{
    LOG_INF("Publishing effort message: left=%d%%, right=%d%%", motor_q31_to_percent(msg->left),
            motor_q31_to_percent(msg->right));
    return zbus_chan_pub(&effort_channel, msg, timeout);
}

bool effort_channel_validator(const void *msg, size_t msg_size)
{
    const struct effort_msg *effort = msg;

    bool valid = (msg_size == sizeof(struct effort_msg)) && motor_effort_q31_is_valid(effort->left)
                 && motor_effort_q31_is_valid(effort->right);

    if (!valid) {
        LOG_WRN("Effort channel validator failed: left=%d%%, right=%d%%, msg_size=%zu",
                motor_q31_to_percent(effort->left), motor_q31_to_percent(effort->right), msg_size);
    }

    return valid;
}

ZBUS_CHAN_DEFINE(effort_channel,
                 struct effort_msg,
                 effort_channel_validator,
                 NULL,
                 ZBUS_OBSERVERS(effort_subscriber),
                 ZBUS_MSG_INIT(.left = 0, .right = 0));
