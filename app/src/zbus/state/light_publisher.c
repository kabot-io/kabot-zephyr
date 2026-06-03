#include "zbus/channels/state_channel.h"
#include "zbus/state_publish_utils.h"

#include <zephyr/device.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(light_publisher, LOG_LEVEL_DBG);

BUILD_ASSERT(DT_HAS_ALIAS(kabot_light_left),
             "No devicetree alias 'kabot-light-left' found for light publisher");
BUILD_ASSERT(DT_NODE_HAS_STATUS_OKAY(DT_ALIAS(kabot_light_left)),
             "Left light devicetree node is not enabled");
BUILD_ASSERT(DT_HAS_ALIAS(kabot_light_right),
             "No devicetree alias 'kabot-light-right' found for light publisher");
BUILD_ASSERT(DT_NODE_HAS_STATUS_OKAY(DT_ALIAS(kabot_light_right)),
             "Right light devicetree node is not enabled");

void light_publisher_task(void)
{
    const struct device *light_left = DEVICE_DT_GET(DT_ALIAS(kabot_light_left));
    const struct device *light_right = DEVICE_DT_GET(DT_ALIAS(kabot_light_right));

    while (!device_is_ready(light_left) || !device_is_ready(light_right)) {
        if (!device_is_ready(light_left)) {
            LOG_ERR("Left light device not ready: %s. Retrying...", light_left->name);
        }
        if (!device_is_ready(light_right)) {
            LOG_ERR("Right light device not ready: %s. Retrying...", light_right->name);
        }
        k_sleep(K_MSEC(CONFIG_KABOT_SENSOR_RETRY_PERIOD_MS));
    }

    LOG_INF("Light publisher active: %d ms (%s, %s)",
            CONFIG_KABOT_STATE_LIGHT_PERIOD_MS,
            light_left->name,
            light_right->name);

    while (true) {
        struct sensor_value left_lux = {0};
        struct sensor_value right_lux = {0};
        bool left_valid = false;
        bool right_valid = false;
        int rc = sensor_sample_fetch(light_left);

        if (rc != 0) {
            LOG_WRN("left sensor_sample_fetch failed: %d", rc);
            k_sleep(K_MSEC(CONFIG_KABOT_STATE_LIGHT_PERIOD_MS));
            continue;
        }

        rc = sensor_sample_fetch(light_right);
        if (rc != 0) {
            LOG_WRN("right sensor_sample_fetch failed: %d", rc);
            k_sleep(K_MSEC(CONFIG_KABOT_STATE_LIGHT_PERIOD_MS));
            continue;
        }

        rc = sensor_channel_get(light_left, SENSOR_CHAN_LIGHT, &left_lux);
        if (should_skip_invalid_sensor_sample(rc)) {
            /* Skip invalid conversion sample without publishing a spike value. */
        } else if (rc != 0) {
            LOG_WRN("sensor_channel_get(left, SENSOR_CHAN_LIGHT) failed: %d", rc);
            k_sleep(K_MSEC(CONFIG_KABOT_STATE_LIGHT_PERIOD_MS));
            continue;
        } else {
            left_valid = true;
        }

        rc = sensor_channel_get(light_right, SENSOR_CHAN_LIGHT, &right_lux);
        if (should_skip_invalid_sensor_sample(rc)) {
            /* Skip invalid conversion sample without publishing a spike value. */
        } else if (rc != 0) {
            LOG_WRN("sensor_channel_get(right, SENSOR_CHAN_LIGHT) failed: %d", rc);
            k_sleep(K_MSEC(CONFIG_KABOT_STATE_LIGHT_PERIOD_MS));
            continue;
        } else {
            right_valid = true;
        }

        if (!left_valid && !right_valid) {
            k_sleep(K_MSEC(CONFIG_KABOT_STATE_LIGHT_PERIOD_MS));
            continue;
        }

        State state = State_init_zero;
        uint64_t stamp = state_now_stamp_ms();

        if (left_valid) {
            state.has_light_left = true;
            state.light_left.has_header = true;
            state.light_left.header.stamp = stamp;
            set_header_frame_id(&state.light_left.header, CONFIG_KABOT_STATE_LIGHT_LEFT_FRAME_ID);
            state.light_left.state = sensor_value_to_float(&left_lux);
        }

        if (right_valid) {
            state.has_light_right = true;
            state.light_right.has_header = true;
            state.light_right.header.stamp = stamp;
            set_header_frame_id(&state.light_right.header, CONFIG_KABOT_STATE_LIGHT_RIGHT_FRAME_ID);
            state.light_right.state = sensor_value_to_float(&right_lux);
        }

        rc = publish_state_msg(&state, K_MSEC(CONFIG_KABOT_STATE_LIGHT_PERIOD_MS));
        if (rc != 0) {
            LOG_WRN("Failed to publish light state: %d", rc);
        }

        k_sleep(K_MSEC(CONFIG_KABOT_STATE_LIGHT_PERIOD_MS));
    }
}

K_THREAD_DEFINE(light_publisher_task_id,
                CONFIG_KABOT_LIGHT_PUBLISHER_STACK_SIZE,
                light_publisher_task,
                NULL,
                NULL,
                NULL,
                4,
                0,
                0);
