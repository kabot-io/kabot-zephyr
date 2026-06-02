#include "motor/motor_math.h"
#include "zbus/control_channel.h"

#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(control_channel, LOG_LEVEL_DBG);

int publish_control_msg(const Control *msg, k_timeout_t timeout)
{
    LOG_INF("Publishing control message: left=%.3f, right=%.3f",
            (double)msg->effort.state.x,
            (double)msg->effort.state.y);
    return zbus_chan_pub(&control_channel, msg, timeout);
}

bool control_channel_validator(const void *msg, size_t msg_size)
{
    if ((msg == NULL) || (msg_size != sizeof(Control))) {
        LOG_WRN("Control channel validator failed: msg=%p, msg_size=%zu", msg, msg_size);
        return false;
    }

    const Control *control = msg;
    const float left = control->effort.state.x;
    const float right = control->effort.state.y;

    bool valid = motor_effort_is_valid(left) && motor_effort_is_valid(right);

    if (!valid) {
        LOG_WRN("Control channel validator failed: left=%.3f, right=%.3f, msg_size=%zu",
                (double)left,
                (double)right,
                msg_size);
    }

    return valid;
}

ZBUS_CHAN_DEFINE(control_channel,
                 Control,
                 control_channel_validator,
                 NULL,
                 ZBUS_OBSERVERS(control_subscriber, effort_state_publisher),
                 ZBUS_MSG_INIT(.effort.state.x = 0.0f, .effort.state.y = 0.0f));
