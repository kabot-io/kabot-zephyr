#include "zbus/channels/state_channel.h"
#include "zbus/state_publish_utils.h"

#include <zephyr/device.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(imu_publisher, LOG_LEVEL_DBG);

enum {
    PUBLISH_TIMEOUT_CAP_MS = 20,
    READY_RETRY_MS = 1000,
};

#if DT_HAS_COMPAT_STATUS_OKAY(invensense_icm42670s)
#define IMU_NODE DT_COMPAT_GET_ANY_STATUS_OKAY(invensense_icm42670s)
#elif DT_HAS_COMPAT_STATUS_OKAY(invensense_icm42670p)
#define IMU_NODE DT_COMPAT_GET_ANY_STATUS_OKAY(invensense_icm42670p)
#endif

BUILD_ASSERT(DT_HAS_COMPAT_STATUS_OKAY(invensense_icm42670s)
                 || DT_HAS_COMPAT_STATUS_OKAY(invensense_icm42670p),
             "IMU publisher requires a devicetree node compatible with invensense,icm42670s or invensense,icm42670p");

static float sensor_value_to_float_unit(const struct sensor_value *value)
{
    return (float)value->val1 + ((float)value->val2 / 1000000.0f);
}

void imu_publisher_task(void)
{
    const struct device *imu = DEVICE_DT_GET(IMU_NODE);
    int publish_timeout_ms = CONFIG_KABOT_STATE_IMU_PERIOD_MS;

    if (publish_timeout_ms > PUBLISH_TIMEOUT_CAP_MS) {
        publish_timeout_ms = PUBLISH_TIMEOUT_CAP_MS;
    }

    while (!device_is_ready(imu)) {
        LOG_ERR("ICM42X70 device not ready: %s. Retrying...", imu->name);
        k_sleep(K_MSEC(READY_RETRY_MS));
    }

    LOG_INF("IMU publisher active: %d ms (%s)", CONFIG_KABOT_STATE_IMU_PERIOD_MS, imu->name);

    while (true) {
        struct sensor_value accel_xyz[3] = {0};
        struct sensor_value gyro_xyz[3] = {0};
        int rc = sensor_sample_fetch(imu);

        if (rc != 0) {
            LOG_WRN("sensor_sample_fetch failed: %d", rc);
            k_sleep(K_MSEC(CONFIG_KABOT_STATE_IMU_PERIOD_MS));
            continue;
        }

        rc = sensor_channel_get(imu, SENSOR_CHAN_ACCEL_XYZ, accel_xyz);
        if (rc != 0) {
            LOG_WRN("sensor_channel_get(SENSOR_CHAN_ACCEL_XYZ) failed: %d", rc);
            k_sleep(K_MSEC(CONFIG_KABOT_STATE_IMU_PERIOD_MS));
            continue;
        }

        rc = sensor_channel_get(imu, SENSOR_CHAN_GYRO_XYZ, gyro_xyz);
        if (rc != 0) {
            LOG_WRN("sensor_channel_get(SENSOR_CHAN_GYRO_XYZ) failed: %d", rc);
            k_sleep(K_MSEC(CONFIG_KABOT_STATE_IMU_PERIOD_MS));
            continue;
        }

        const uint64_t stamp = state_now_stamp_ms();

        State state = State_init_zero;

        state.has_linear_acceleration = true;
        state.linear_acceleration.has_header = true;
        state.linear_acceleration.header.stamp = stamp;
        set_header_frame_id(&state.linear_acceleration.header, CONFIG_KABOT_STATE_IMU_FRAME_ID);
        state.linear_acceleration.has_state = true;
        state.linear_acceleration.state.x = sensor_value_to_float_unit(&accel_xyz[0]);
        state.linear_acceleration.state.y = sensor_value_to_float_unit(&accel_xyz[1]);
        state.linear_acceleration.state.z = sensor_value_to_float_unit(&accel_xyz[2]);

        state.has_angular_velocity = true;
        state.angular_velocity.has_header = true;
        state.angular_velocity.header.stamp = stamp;
        set_header_frame_id(&state.angular_velocity.header, CONFIG_KABOT_STATE_IMU_FRAME_ID);
        state.angular_velocity.has_state = true;
        state.angular_velocity.state.x = sensor_value_to_float_unit(&gyro_xyz[0]);
        state.angular_velocity.state.y = sensor_value_to_float_unit(&gyro_xyz[1]);
        state.angular_velocity.state.z = sensor_value_to_float_unit(&gyro_xyz[2]);

        rc = publish_state_msg(&state, K_MSEC(publish_timeout_ms));
        if (rc != 0) {
            LOG_WRN("Failed to publish IMU state: %d", rc);
        }

        k_sleep(K_MSEC(CONFIG_KABOT_STATE_IMU_PERIOD_MS));
    }
}

K_THREAD_DEFINE(imu_publisher_task_id,
                CONFIG_KABOT_IMU_PUBLISHER_STACK_SIZE,
                imu_publisher_task,
                NULL,
                NULL,
                NULL,
                4,
                0,
                0);
