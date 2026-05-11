
#include "zbus/effort_subscriber.h"
#include "zbus/effort_channel.h"
#include "motor_driver.h"
#include "motor_registry.h"

#include <errno.h>
#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(effort_subscriber, LOG_LEVEL_DBG);

/* --- Subscriber task ----------------------------------------------------- */

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
            const struct motor_registry_entry *left = motor_registry_find("left");
            const struct motor_registry_entry *right = motor_registry_find("right");

            int left_result = left ? motor_driver_set_effort(left->drv, effort.left) : -ENOENT;
            int right_result = right ? motor_driver_set_effort(right->drv, effort.right) : -ENOENT;

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

/* --- Initialization ------------------------------------------------------ */

int initialize_motor_drivers(void)
{
    size_t count = motor_registry_count();

    for (size_t i = 0; i < count; i++) {
        const struct motor_registry_entry *entry = motor_registry_get(i);
        int ret = motor_driver_init(entry->drv);
        if (ret < 0) {
            LOG_ERR("Failed to init motor '%s': %d", entry->name, ret);
            return ret;
        }
    }
    return 0;
}
