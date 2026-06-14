#include "zbus/channels/state_channel.h"
#include "zbus/state_publish_utils.h"

#include <zephyr/device.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(imu_publisher, LOG_LEVEL_DBG);

BUILD_ASSERT(DT_HAS_ALIAS(kabot_imu), "No devicetree alias 'kabot-imu' found for IMU publisher");
BUILD_ASSERT(DT_NODE_HAS_STATUS_OKAY(DT_ALIAS(kabot_imu)), "IMU devicetree node is not enabled");

void imu_publisher_task(void)
{
	const struct device *imu = DEVICE_DT_GET(DT_ALIAS(kabot_imu));

	while (!device_is_ready(imu)) {
		LOG_ERR("ICM42X70 device not ready: %s. Retrying...", imu->name);
		k_sleep(K_MSEC(CONFIG_KABOT_SENSOR_RETRY_PERIOD_MS));
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
		if (should_skip_invalid_sensor_sample(rc)) {
			LOG_DBG("Skipping invalid acceleration sample");
			k_sleep(K_MSEC(CONFIG_KABOT_STATE_IMU_PERIOD_MS));
			continue;
		}

		if (rc != 0) {
			LOG_WRN("sensor_channel_get(SENSOR_CHAN_ACCEL_XYZ) failed: %d", rc);
			k_sleep(K_MSEC(CONFIG_KABOT_STATE_IMU_PERIOD_MS));
			continue;
		}

		rc = sensor_channel_get(imu, SENSOR_CHAN_GYRO_XYZ, gyro_xyz);
		if (should_skip_invalid_sensor_sample(rc)) {
			LOG_DBG("Skipping invalid angular velocity sample");
			k_sleep(K_MSEC(CONFIG_KABOT_STATE_IMU_PERIOD_MS));
			continue;
		}

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
		set_header_frame_id(&state.linear_acceleration.header,
				    CONFIG_KABOT_STATE_IMU_FRAME_ID);
		state.linear_acceleration.has_state = true;
		state.linear_acceleration.state.x = sensor_value_to_float(&accel_xyz[0]);
		state.linear_acceleration.state.y = sensor_value_to_float(&accel_xyz[1]);
		state.linear_acceleration.state.z = sensor_value_to_float(&accel_xyz[2]);

		state.has_angular_velocity = true;
		state.angular_velocity.has_header = true;
		state.angular_velocity.header.stamp = stamp;
		set_header_frame_id(&state.angular_velocity.header,
				    CONFIG_KABOT_STATE_IMU_FRAME_ID);
		state.angular_velocity.has_state = true;
		state.angular_velocity.state.x = sensor_value_to_float(&gyro_xyz[0]);
		state.angular_velocity.state.y = sensor_value_to_float(&gyro_xyz[1]);
		state.angular_velocity.state.z = sensor_value_to_float(&gyro_xyz[2]);

		rc = publish_state_msg(&state, K_MSEC(CONFIG_KABOT_STATE_IMU_PERIOD_MS));
		if (rc != 0) {
			LOG_WRN("Failed to publish IMU state: %d", rc);
		}

		k_sleep(K_MSEC(CONFIG_KABOT_STATE_IMU_PERIOD_MS));
	}
}

K_THREAD_DEFINE(imu_publisher_task_id, CONFIG_KABOT_IMU_PUBLISHER_STACK_SIZE, imu_publisher_task,
		NULL, NULL, NULL, 4, 0, 0);
