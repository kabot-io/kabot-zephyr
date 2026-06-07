#include "zbus/channels/state_channel.h"
#include "zbus/state_publish_utils.h"

#include <zephyr/device.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(distance_publisher, LOG_LEVEL_DBG);

BUILD_ASSERT(DT_HAS_ALIAS(kabot_distance), "No devicetree alias 'kabot-distance' found for distance publisher");
BUILD_ASSERT(DT_NODE_HAS_STATUS_OKAY(DT_ALIAS(kabot_distance)), "Distance devicetree node is not enabled");


void distance_publisher_task(void)
{
    const struct device *tof = DEVICE_DT_GET(DT_ALIAS(kabot_distance));

    while (!device_is_ready(tof)) {
        LOG_ERR("VL53L0X device not ready: %s. Retrying...", tof->name);
        k_sleep(K_MSEC(CONFIG_KABOT_SENSOR_RETRY_PERIOD_MS));
    }

    LOG_INF("Distance publisher active: %d ms (%s)",
        CONFIG_KABOT_STATE_DISTANCE_PERIOD_MS,
        tof->name);

    while (true) {
        struct sensor_value distance = {0};
        int rc = sensor_sample_fetch(tof);

        if (rc != 0) {
            LOG_WRN("sensor_sample_fetch failed: %d", rc);
            k_sleep(K_MSEC(CONFIG_KABOT_STATE_DISTANCE_PERIOD_MS));
            continue;
        }

        rc = sensor_channel_get(tof, SENSOR_CHAN_DISTANCE, &distance);
        if (should_skip_invalid_sensor_sample(rc)) {
            LOG_DBG("Skipping invalid distance sample");
            k_sleep(K_MSEC(CONFIG_KABOT_STATE_DISTANCE_PERIOD_MS));
            continue;
        }

        if (rc != 0) {
            LOG_WRN("sensor_channel_get(SENSOR_CHAN_DISTANCE) failed: %d", rc);
            k_sleep(K_MSEC(CONFIG_KABOT_STATE_DISTANCE_PERIOD_MS));
            continue;
        }

        State state = State_init_zero;
        state.has_distance = true;
        state.distance.has_header = true;
        state.distance.header.stamp = state_now_stamp_ms();
        set_header_frame_id(&state.distance.header, CONFIG_KABOT_STATE_DISTANCE_FRAME_ID);
        state.distance.state = sensor_value_to_float(&distance);
        if (state.distance.state > 1.0f) {
            state.distance.state = 1.0f;
        }

        rc = publish_state_msg(&state, K_MSEC(CONFIG_KABOT_STATE_DISTANCE_PERIOD_MS));
        if (rc != 0) {
            LOG_WRN("Failed to publish distance state: %d", rc);
        }

        k_sleep(K_MSEC(CONFIG_KABOT_STATE_DISTANCE_PERIOD_MS));
    }
}

K_THREAD_DEFINE(distance_publisher_task_id,
                CONFIG_KABOT_DISTANCE_PUBLISHER_STACK_SIZE,
                distance_publisher_task,
                NULL,
                NULL,
                NULL,
                4,
                0,
                0);
