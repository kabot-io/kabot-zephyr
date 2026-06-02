#include "motor/motor_driver.h"
#include "zbus/control_channel.h"
#include "zbus/control_subscriber.h"

#include <errno.h>
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(control_subscriber, LOG_LEVEL_DBG);

#define MOTOR_LEFT_NODE  DT_ALIAS(motor_left)
#define MOTOR_RIGHT_NODE DT_ALIAS(motor_right)

BUILD_ASSERT(DT_NODE_EXISTS(MOTOR_LEFT_NODE),
             "Board overlay must provide a 'motor-left' alias for motor device.");
BUILD_ASSERT(DT_NODE_EXISTS(MOTOR_RIGHT_NODE),
             "Board overlay must provide a 'motor-right' alias for motor device.");

#define MOTOR_LEFT_DEV  DEVICE_DT_GET(MOTOR_LEFT_NODE)
#define MOTOR_RIGHT_DEV DEVICE_DT_GET(MOTOR_RIGHT_NODE)

ZBUS_SUBSCRIBER_DEFINE(control_subscriber, 1);

void control_subscriber_task(void)
{
    const struct zbus_channel *chan;

    if (!device_is_ready(MOTOR_LEFT_DEV)) {
        LOG_ERR("left motor device not ready");
        return;
    }

    if (!device_is_ready(MOTOR_RIGHT_DEV)) {
        LOG_ERR("right motor device not ready");
        return;
    }

    LOG_INF("Starting subscriber on control channel");

    while (!zbus_sub_wait(&control_subscriber, &chan, K_FOREVER)) {
        if (&control_channel != chan) {
            continue;
        }

        Control control;
        if (zbus_chan_read(&control_channel, &control, K_MSEC(20)) == 0) {
            const float left_effort = control.effort.state.x;
            const float right_effort = control.effort.state.y;

            LOG_INF("From subscriber -> Left effort=%.3f, Right effort=%.3f", left_effort,
                    right_effort);

            int left_result = motor_set_effort(MOTOR_LEFT_DEV, left_effort);
            int right_result = motor_set_effort(MOTOR_RIGHT_DEV, right_effort);

            if (left_result < 0 || right_result < 0) {
                LOG_WRN("Failed to set motor effort: L=%d, R=%d", left_result, right_result);
            }
        } else {
            LOG_WRN("Failed to read from control_channel");
        }
    }
}

K_THREAD_DEFINE(control_subscriber_task_id, CONFIG_MAIN_STACK_SIZE, control_subscriber_task, NULL,
                NULL, NULL, 3, 0, 0);
