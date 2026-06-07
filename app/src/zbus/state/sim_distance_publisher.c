#include "zbus/channels/state_channel.h"
#include "zbus/state_publish_utils.h"

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(sim_distance_publisher, LOG_LEVEL_DBG);

void sim_distance_publisher_task(void)
{
    LOG_INF("Sim distance publisher active: %d ms", CONFIG_KABOT_STATE_DISTANCE_PERIOD_MS);

    while (true) {
        const uint64_t stamp = state_now_stamp_ms();
        State state = State_init_zero;

        state.has_distance = true;
        state.distance.has_header = true;
        state.distance.header.stamp = stamp;
        set_header_frame_id(&state.distance.header, CONFIG_KABOT_STATE_DISTANCE_FRAME_ID);
        state.distance.state = random_rangef(0.05f, 1.0f);

        int rc = publish_state_msg(&state, K_MSEC(CONFIG_KABOT_STATE_DISTANCE_PERIOD_MS));
        if (rc != 0) {
            LOG_WRN("Failed to publish simulated distance state: %d", rc);
        }

        k_sleep(K_MSEC(CONFIG_KABOT_STATE_DISTANCE_PERIOD_MS));
    }
}

K_THREAD_DEFINE(sim_distance_publisher_task_id,
                CONFIG_MAIN_STACK_SIZE,
                sim_distance_publisher_task,
                NULL,
                NULL,
                NULL,
                4,
                0,
                0);
