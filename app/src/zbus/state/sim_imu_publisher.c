#include "zbus/channels/state_channel.h"
#include "zbus/state_publish_utils.h"

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(sim_imu_publisher, LOG_LEVEL_DBG);

enum { PUBLISH_TIMEOUT_MS = 20 };

void sim_imu_publisher_task(void)
{
    LOG_INF("Sim IMU publisher active: %d ms", CONFIG_KABOT_STATE_IMU_PERIOD_MS);

    while (true) {
        const uint64_t stamp = state_now_stamp_ms();

        State state = State_init_zero;

        state.has_linear_acceleration = true;
        state.linear_acceleration.has_header = true;
        state.linear_acceleration.header.stamp = stamp;
        set_header_frame_id(&state.linear_acceleration.header, CONFIG_KABOT_STATE_IMU_FRAME_ID);
        state.linear_acceleration.has_state = true;
        state.linear_acceleration.state.x = random_rangef(-2.0f, 2.0f);
        state.linear_acceleration.state.y = random_rangef(-2.0f, 2.0f);
        state.linear_acceleration.state.z = random_rangef(-2.0f, 2.0f);

        state.has_angular_velocity = true;
        state.angular_velocity.has_header = true;
        state.angular_velocity.header.stamp = stamp;
        set_header_frame_id(&state.angular_velocity.header, CONFIG_KABOT_STATE_IMU_FRAME_ID);
        state.angular_velocity.has_state = true;
        state.angular_velocity.state.x = random_rangef(-1.5f, 1.5f);
        state.angular_velocity.state.y = random_rangef(-1.5f, 1.5f);
        state.angular_velocity.state.z = random_rangef(-1.5f, 1.5f);

        int rc = publish_state_msg(&state, K_MSEC(PUBLISH_TIMEOUT_MS));
        if (rc != 0) {
            LOG_WRN("Failed to publish simulated IMU state: %d", rc);
        }

        k_sleep(K_MSEC(CONFIG_KABOT_STATE_IMU_PERIOD_MS));
    }
}

K_THREAD_DEFINE(sim_imu_publisher_task_id,
                CONFIG_MAIN_STACK_SIZE,
                sim_imu_publisher_task,
                NULL,
                NULL,
                NULL,
                4,
                0,
                0);
