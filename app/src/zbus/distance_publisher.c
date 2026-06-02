#include "zbus/state_channel.h"
#include "zbus/state_publish_utils.h"

#include <zephyr/device.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(distance_publisher, LOG_LEVEL_DBG);

enum {
    PUBLISH_TIMEOUT_CAP_MS = 20,
};

#define TOF_NODE DT_COMPAT_GET_ANY_STATUS_OKAY(st_vl53l0x)

BUILD_ASSERT(DT_HAS_COMPAT_STATUS_OKAY(st_vl53l0x),
             "Distance publisher requires a devicetree node compatible with st,vl53l0x");

static float sensor_value_to_float_meters(const struct sensor_value *value)
{
    return (float)value->val1 + ((float)value->val2 / 1000000.0f);
}

void distance_publisher_task(void)
{
    const struct device *tof = DEVICE_DT_GET(TOF_NODE);
    int publish_timeout_ms = CONFIG_KABOT_STATE_DISTANCE_PERIOD_MS;

    if (publish_timeout_ms > PUBLISH_TIMEOUT_CAP_MS) {
        publish_timeout_ms = PUBLISH_TIMEOUT_CAP_MS;
    }

    if (!device_is_ready(tof)) {
        LOG_ERR("VL53L0X device not ready: %s", tof->name);
        return;
    }

    LOG_INF("Distance publisher active: %d ms (%s)",
            CONFIG_KABOT_STATE_DISTANCE_PERIOD_MS,
            tof->name);

    while (true) {
        struct sensor_value distance_value = {0};
        int rc = sensor_sample_fetch(tof);

        if (rc != 0) {
            LOG_WRN("sensor_sample_fetch failed: %d", rc);
            k_sleep(K_MSEC(CONFIG_KABOT_STATE_DISTANCE_PERIOD_MS));
            continue;
        }

        rc = sensor_channel_get(tof, SENSOR_CHAN_DISTANCE, &distance_value);
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
        state.distance.state = sensor_value_to_float_meters(&distance_value);

        rc = publish_state_msg(&state, K_MSEC(publish_timeout_ms));
        if (rc != 0) {
            LOG_WRN("Failed to publish distance state: %d", rc);
        }

        k_sleep(K_MSEC(CONFIG_KABOT_STATE_DISTANCE_PERIOD_MS));
    }
}

K_THREAD_DEFINE(distance_publisher_task_id,
                CONFIG_MAIN_STACK_SIZE,
                distance_publisher_task,
                NULL,
                NULL,
                NULL,
                4,
                0,
                0);
