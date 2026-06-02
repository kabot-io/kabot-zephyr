#include "zbus/control_channel.h"
#include "zbus/state_channel.h"

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(effort_state_publisher, LOG_LEVEL_DBG);

ZBUS_SUBSCRIBER_DEFINE(effort_state_publisher, 1);

enum { ZBUS_READ_TIMEOUT_MS = 20 };

void effort_state_publisher_task(void)
{
    const struct zbus_channel *chan;

    LOG_INF("Starting effort_state_publisher (control -> state)");

    while (!zbus_sub_wait(&effort_state_publisher, &chan, K_FOREVER)) {
        if (&control_channel != chan) {
            continue;
        }

        Control control;
        if (zbus_chan_read(&control_channel, &control, K_MSEC(ZBUS_READ_TIMEOUT_MS)) != 0) {
            LOG_WRN("Failed to read from control_channel");
            continue;
        }

        State state = State_init_zero;
        const uint64_t stamp = (uint64_t)k_uptime_get();

        state.has_header = true;
        state.header.stamp = stamp;

        state.has_effort = true;
        state.effort.has_header = true;
        state.effort.header.stamp = stamp;
        state.effort.has_state = true;
        state.effort.state.x = control.effort.state.x;
        state.effort.state.y = control.effort.state.y;

        int rc = publish_state_msg(&state, K_MSEC(ZBUS_READ_TIMEOUT_MS));
        if (rc != 0) {
            LOG_WRN("Failed to publish state message: %d", rc);
        }
    }
}

K_THREAD_DEFINE(effort_state_publisher_task_id,
                CONFIG_MAIN_STACK_SIZE,
                effort_state_publisher_task,
                NULL,
                NULL,
                NULL,
                4,
                0,
                0);
