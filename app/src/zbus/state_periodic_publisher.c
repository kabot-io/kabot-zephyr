#include "zbus/state_aggregator.h"
#include "zbus/state_egress_channel.h"
#include "zbus/state_publish_utils.h"

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(state_periodic_publisher, LOG_LEVEL_DBG);

enum {
    PUBLISH_TIMEOUT_MS = 20,
};

void state_periodic_publisher_task(void)
{
    int64_t next_publish_at_ms = k_uptime_get();

    LOG_INF("State periodic publisher active: %d ms", CONFIG_KABOT_STATE_EGRESS_PERIOD_MS);

    while (true) {
        int64_t now_ms = k_uptime_get();
        int64_t wait_ms = next_publish_at_ms - now_ms;
        if (wait_ms > 0) {
            k_sleep(K_MSEC(wait_ms));
        }

        now_ms = k_uptime_get();
        next_publish_at_ms += CONFIG_KABOT_STATE_EGRESS_PERIOD_MS;
        if (next_publish_at_ms <= now_ms) {
            next_publish_at_ms = now_ms + CONFIG_KABOT_STATE_EGRESS_PERIOD_MS;
        }

        State to_send = State_init_zero;
        state_aggregator_get_snapshot(&to_send);
        to_send.has_header = true;
        to_send.header.stamp = (uint64_t)now_ms;
        set_header_frame_id(&to_send.header, CONFIG_KABOT_STATE_TOP_FRAME_ID);

        int rc = publish_state_egress_msg(&to_send, K_MSEC(PUBLISH_TIMEOUT_MS));
        if (rc != 0) {
            LOG_WRN("Failed to publish state_egress message: %d", rc);
        }
    }
}

K_THREAD_DEFINE(state_periodic_publisher_task_id,
                CONFIG_MAIN_STACK_SIZE,
                state_periodic_publisher_task,
                NULL,
                NULL,
                NULL,
                4,
                0,
                0);
