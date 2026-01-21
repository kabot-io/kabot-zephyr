#include "zbus/effort_channel.h"
#include "zbus/effort_msg.h"

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(effort_channel, LOG_LEVEL_DBG);

bool publish_effort_msg(const struct effort_msg *msg, k_timeout_t timeout)
{
    LOG_INF("Publishing effort message: left=%d, right=%d", msg->left, msg->right);
    return zbus_chan_pub(&effort_channel, msg, timeout);
}

bool effort_channel_validator(const void *msg, size_t msg_size)
{
    const struct effort_msg *effort = msg;

    bool valid = (msg_size == sizeof(struct effort_msg)) &&
                 (effort->left >= -100 && effort->left <= 100) &&
                 (effort->right >= -100 && effort->right <= 100);

    if (!valid) {
        LOG_WRN("Effort channel validator failed: left=%d, right=%d, msg_size=%zu",
                effort->left, effort->right, msg_size);
    }

    return valid;
}

ZBUS_CHAN_DEFINE(effort_channel,
                 struct effort_msg,
                 effort_channel_validator,
                 NULL, // This could be used for info about mapping to ROS2 topic
                 ZBUS_OBSERVERS(effort_subscriber),
                 ZBUS_MSG_INIT(.left = 0, .right = 0));
