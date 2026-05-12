#include "motor/motor_driver.h"
#include "motor/motor_math.h"
#include "zbus/effort_channel.h"
#include "zbus/effort_subscriber.h"

#include <errno.h>
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(effort_subscriber, LOG_LEVEL_DBG);

#define MOTOR_LEFT_NODE  DT_ALIAS(motor_left)
#define MOTOR_RIGHT_NODE DT_ALIAS(motor_right)

#define MOTOR_NODE_IS_SUPPORTED(node_id)                                                          \
    (DT_NODE_HAS_COMPAT(node_id, kabot_esc) || DT_NODE_HAS_COMPAT(node_id, kabot_sim_motor))

BUILD_ASSERT(DT_NODE_EXISTS(MOTOR_LEFT_NODE),
             "Board overlay must provide a 'motor-left' alias for motor device.");
BUILD_ASSERT(DT_NODE_EXISTS(MOTOR_RIGHT_NODE),
             "Board overlay must provide a 'motor-right' alias for motor device.");

BUILD_ASSERT(MOTOR_NODE_IS_SUPPORTED(MOTOR_LEFT_NODE),
             "Alias 'motor-left' must target a node compatible with motor API.");
BUILD_ASSERT(MOTOR_NODE_IS_SUPPORTED(MOTOR_RIGHT_NODE),
             "Alias 'motor-right' must target a node compatible with motor API.");

#define MOTOR_LEFT_DEV  DEVICE_DT_GET(MOTOR_LEFT_NODE)
#define MOTOR_RIGHT_DEV DEVICE_DT_GET(MOTOR_RIGHT_NODE)

ZBUS_SUBSCRIBER_DEFINE(effort_subscriber, 1);

void effort_subscriber_task(void)
{
    const struct zbus_channel *chan;
    LOG_INF("Starting subscriber on effort channel");

    while (!zbus_sub_wait(&effort_subscriber, &chan, K_FOREVER)) {
        if (&effort_channel != chan) {
            continue;
        }

        struct effort_msg effort;
        if (zbus_chan_read(&effort_channel, &effort, K_MSEC(20)) == 0) {
            LOG_INF("From subscriber -> Left effort=%d%%, Right effort=%d%%",
                    motor_q31_to_percent(effort.left), motor_q31_to_percent(effort.right));

            int left_result = motor_set_effort(MOTOR_LEFT_DEV, effort.left);
            int right_result = motor_set_effort(MOTOR_RIGHT_DEV, effort.right);

            if (left_result < 0 || right_result < 0) {
                LOG_WRN("Failed to set motor effort: L=%d, R=%d", left_result, right_result);
            }
        } else {
            LOG_WRN("Failed to read from effort_channel");
        }
    }
}

K_THREAD_DEFINE(effort_subscriber_task_id, CONFIG_MAIN_STACK_SIZE, effort_subscriber_task, NULL,
                NULL, NULL, 3, 0, 0);

int initialize_motor_drivers(void)
{
    if (!device_is_ready(MOTOR_LEFT_DEV)) {
        LOG_ERR("Left motor device not ready: %s", MOTOR_LEFT_DEV->name);
        return -ENODEV;
    }

    if (!device_is_ready(MOTOR_RIGHT_DEV)) {
        LOG_ERR("Right motor device not ready: %s", MOTOR_RIGHT_DEV->name);
        return -ENODEV;
    }

    return 0;
}