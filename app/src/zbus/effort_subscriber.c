
#include "zbus/effort_subscriber.h"
#include "zbus/effort_channel.h"
#include "motor_driver.h"

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(effort_subscriber, LOG_LEVEL_DBG);

/*
 * Select the concrete motor driver backend at build time.
 *
 * - Real hardware (e.g. ESP32S3): ESC nodes are present in the devicetree →
 *   use the PWM ESC backend.
 * - native_sim / future H-Bridge boards without ESC nodes: fall back to the
 *   simulation stub, which logs effort values and is designed to be extended
 *   into a Gazebo / ROS 2 bridge.
 */
#if DT_NODE_EXISTS(DT_NODELABEL(esc_left)) && DT_NODE_EXISTS(DT_NODELABEL(esc_right))
#include "esc_driver.h"

static const struct esc_driver left_drv = ESC_DRIVER_FROM_DT(DT_NODELABEL(esc_left), true);
static const struct esc_driver right_drv = ESC_DRIVER_FROM_DT(DT_NODELABEL(esc_right), false);

#define LEFT_MOTOR  ((const struct motor_driver *)&left_drv)
#define RIGHT_MOTOR ((const struct motor_driver *)&right_drv)
#else
#include "sim_motor_driver.h"

static const struct sim_motor_driver left_drv = SIM_MOTOR_DRIVER_DEFINE("left");
static const struct sim_motor_driver right_drv = SIM_MOTOR_DRIVER_DEFINE("right");

#define LEFT_MOTOR  ((const struct motor_driver *)&left_drv)
#define RIGHT_MOTOR ((const struct motor_driver *)&right_drv)
#endif /* DT_NODE_EXISTS(esc_left) && DT_NODE_EXISTS(esc_right) */

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
            LOG_INF("From subscriber -> Left effort=%d, Right effort=%d", effort.left,
                    effort.right);
            (void)motor_driver_set_effort(LEFT_MOTOR, effort.left);
            (void)motor_driver_set_effort(RIGHT_MOTOR, effort.right);
        } else {
            LOG_WRN("Failed to read from effort_channel");
        }
    }
}
K_THREAD_DEFINE(effort_subscriber_task_id, CONFIG_MAIN_STACK_SIZE, effort_subscriber_task, NULL,
                NULL, NULL, 3, 0, 0);

int initialize_motor_drivers(void)
{
    int ret = motor_driver_init(LEFT_MOTOR);
    if (ret < 0) {
        return ret;
    }

    return motor_driver_init(RIGHT_MOTOR);
}
