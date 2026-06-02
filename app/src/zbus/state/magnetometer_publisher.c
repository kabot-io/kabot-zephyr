#include "zbus/channels/state_channel.h"
#include "zbus/state_publish_utils.h"

#include <zephyr/device.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(magnetometer_publisher, LOG_LEVEL_DBG);

enum {
    PUBLISH_TIMEOUT_CAP_MS = 20,
    READY_RETRY_MS = 1000,
};

#define MAG_NODE DT_COMPAT_GET_ANY_STATUS_OKAY(memsic_mmc56x3)

BUILD_ASSERT(DT_HAS_COMPAT_STATUS_OKAY(memsic_mmc56x3),
             "Magnetometer publisher requires a devicetree node compatible with memsic,mmc56x3");

static float sensor_value_to_float_gauss(const struct sensor_value *value)
{
    return (float)value->val1 + ((float)value->val2 / 1000000.0f);
}

void magnetometer_publisher_task(void)
{
    const struct device *mag = DEVICE_DT_GET(MAG_NODE);
    int publish_timeout_ms = CONFIG_KABOT_STATE_MAG_PERIOD_MS;

    if (publish_timeout_ms > PUBLISH_TIMEOUT_CAP_MS) {
        publish_timeout_ms = PUBLISH_TIMEOUT_CAP_MS;
    }

    while (!device_is_ready(mag)) {
        LOG_ERR("MMC56X3 device not ready: %s. Retrying...", mag->name);
        k_sleep(K_MSEC(READY_RETRY_MS));
    }

    LOG_INF("Magnetometer publisher active: %d ms (%s)",
            CONFIG_KABOT_STATE_MAG_PERIOD_MS,
            mag->name);

    while (true) {
        struct sensor_value magnetic_xyz[3] = {0};
        int rc = sensor_sample_fetch(mag);

        if (rc != 0) {
            LOG_WRN("sensor_sample_fetch failed: %d", rc);
            k_sleep(K_MSEC(CONFIG_KABOT_STATE_MAG_PERIOD_MS));
            continue;
        }

        rc = sensor_channel_get(mag, SENSOR_CHAN_MAGN_XYZ, magnetic_xyz);
        if (rc != 0) {
            LOG_WRN("sensor_channel_get(SENSOR_CHAN_MAGN_XYZ) failed: %d", rc);
            k_sleep(K_MSEC(CONFIG_KABOT_STATE_MAG_PERIOD_MS));
            continue;
        }

        State state = State_init_zero;
        state.has_magnetic_field = true;
        state.magnetic_field.has_header = true;
        state.magnetic_field.header.stamp = state_now_stamp_ms();
        set_header_frame_id(&state.magnetic_field.header, CONFIG_KABOT_STATE_MAG_FRAME_ID);
        state.magnetic_field.has_state = true;

        /* Zephyr magnetometer values are reported in gauss; publish as microtesla for State parity. */
        state.magnetic_field.state.x = sensor_value_to_float_gauss(&magnetic_xyz[0]) * 100.0f;
        state.magnetic_field.state.y = sensor_value_to_float_gauss(&magnetic_xyz[1]) * 100.0f;
        state.magnetic_field.state.z = sensor_value_to_float_gauss(&magnetic_xyz[2]) * 100.0f;

        rc = publish_state_msg(&state, K_MSEC(publish_timeout_ms));
        if (rc != 0) {
            LOG_WRN("Failed to publish magnetometer state: %d", rc);
        }

        k_sleep(K_MSEC(CONFIG_KABOT_STATE_MAG_PERIOD_MS));
    }
}

K_THREAD_DEFINE(magnetometer_publisher_task_id,
                CONFIG_KABOT_MAGNETOMETER_PUBLISHER_STACK_SIZE,
                magnetometer_publisher_task,
                NULL,
                NULL,
                NULL,
                4,
                0,
                0);
