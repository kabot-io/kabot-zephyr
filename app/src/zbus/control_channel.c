#include "motor/motor_math.h"
#include "zbus/control_channel.h"

#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(control_channel, LOG_LEVEL_DBG);

int publish_control_msg(const Control *msg, k_timeout_t timeout)
{
    LOG_INF("Publishing control message: left=%.3f, right=%.3f", msg->effort.state.x,
            msg->effort.state.y);
    return zbus_chan_pub(&control_channel, msg, timeout);
}

bool control_channel_validator(const void *msg, size_t msg_size)
{
    const Control *control = msg;
    const float left = control->effort.state.x;
    const float right = control->effort.state.y;

    bool valid = (msg_size == sizeof(Control)) && motor_effort_is_valid(left)
                 && motor_effort_is_valid(right);

    if (!valid) {
        LOG_WRN("Control channel validator failed: left=%.3f, right=%.3f, msg_size=%zu",
                left, right, msg_size);
    }

    return valid;
}

ZBUS_CHAN_DEFINE(control_channel,
                 Control,
                 control_channel_validator,
                 NULL,
                 ZBUS_OBSERVERS(control_subscriber),
                 ZBUS_MSG_INIT(.effort.state.x = 0.0f, .effort.state.y = 0.0f));
