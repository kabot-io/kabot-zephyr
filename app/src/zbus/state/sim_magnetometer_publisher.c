#include "zbus/channels/state_channel.h"
#include "zbus/state_publish_utils.h"

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(sim_magnetometer_publisher, LOG_LEVEL_DBG);

enum { PUBLISH_TIMEOUT_MS = 20 };

void sim_magnetometer_publisher_task(void)
{
    LOG_INF("Sim magnetometer publisher active: %d ms", CONFIG_KABOT_STATE_MAG_PERIOD_MS);

    while (true) {
        const uint64_t stamp = state_now_stamp_ms();
        State state = State_init_zero;

        state.has_magnetic_field = true;
        state.magnetic_field.has_header = true;
        state.magnetic_field.header.stamp = stamp;
        set_header_frame_id(&state.magnetic_field.header, CONFIG_KABOT_STATE_MAG_FRAME_ID);
        state.magnetic_field.has_state = true;
        state.magnetic_field.state.x = random_rangef(-60.0f, 60.0f);
        state.magnetic_field.state.y = random_rangef(-60.0f, 60.0f);
        state.magnetic_field.state.z = random_rangef(-60.0f, 60.0f);

        int rc = publish_state_msg(&state, K_MSEC(PUBLISH_TIMEOUT_MS));
        if (rc != 0) {
            LOG_WRN("Failed to publish simulated magnetic field state: %d", rc);
        }

        k_sleep(K_MSEC(CONFIG_KABOT_STATE_MAG_PERIOD_MS));
    }
}

K_THREAD_DEFINE(sim_magnetometer_publisher_task_id,
                CONFIG_MAIN_STACK_SIZE,
                sim_magnetometer_publisher_task,
                NULL,
                NULL,
                NULL,
                4,
                0,
                0);
