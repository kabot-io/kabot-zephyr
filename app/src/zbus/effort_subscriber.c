
#include "zbus/effort_subscriber.h"
#include "zbus/effort_channel.h"
#include "esc_driver.h"

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(effort_subscriber, LOG_LEVEL_DBG);

#if DT_NODE_EXISTS(DT_NODELABEL(esc_left)) && DT_NODE_EXISTS(DT_NODELABEL(esc_right))
#define ESC_LEFT  DT_NODELABEL(esc_left)
#define ESC_RIGHT DT_NODELABEL(esc_right)

static const struct esc_driver left_esc = ESC_DRIVER_FROM_DT(ESC_LEFT, true);
static const struct esc_driver right_esc = ESC_DRIVER_FROM_DT(ESC_RIGHT, false);
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
#if DT_NODE_EXISTS(DT_NODELABEL(esc_left)) && DT_NODE_EXISTS(DT_NODELABEL(esc_right))
            (void)esc_driver_set_effort(&left_esc, effort.left);
            (void)esc_driver_set_effort(&right_esc, effort.right);
#endif /* DT_NODE_EXISTS(esc_left) && DT_NODE_EXISTS(esc_right) */
        } else {
            LOG_WRN("Failed to read from effort_channel");
        }
    }
}
K_THREAD_DEFINE(effort_subscriber_task_id, CONFIG_MAIN_STACK_SIZE, effort_subscriber_task, NULL,
                NULL, NULL, 3, 0, 0);

int initialize_motor_pwms(void)
{
#if DT_NODE_EXISTS(DT_NODELABEL(esc_left)) && DT_NODE_EXISTS(DT_NODELABEL(esc_right))
    int ret = esc_driver_init(&left_esc);
    if (ret < 0) {
        return ret;
    }

    return esc_driver_init(&right_esc);
#else
    return 0;
#endif /* DT_NODE_EXISTS(esc_left) && DT_NODE_EXISTS(esc_right) */
}
