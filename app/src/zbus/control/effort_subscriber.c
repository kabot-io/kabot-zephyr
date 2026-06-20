#include "motor/motor_driver.h"
#include "zbus/channels/control_channel.h"
#include "zbus/control/effort_subscriber.h"

#include <errno.h>
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/logging/log.h>
#include "system/robot_settings.h"

LOG_MODULE_REGISTER(effort_subscriber, LOG_LEVEL_DBG);

#define MOTOR_LEFT_NODE  DT_ALIAS(motor_left)
#define MOTOR_RIGHT_NODE DT_ALIAS(motor_right)

BUILD_ASSERT(DT_NODE_EXISTS(MOTOR_LEFT_NODE),
	     "Board overlay must provide a 'motor-left' alias for motor device.");
BUILD_ASSERT(DT_NODE_EXISTS(MOTOR_RIGHT_NODE),
	     "Board overlay must provide a 'motor-right' alias for motor device.");

#define MOTOR_LEFT_DEV  DEVICE_DT_GET(MOTOR_LEFT_NODE)
#define MOTOR_RIGHT_DEV DEVICE_DT_GET(MOTOR_RIGHT_NODE)

ZBUS_SUBSCRIBER_DEFINE(effort_subscriber, 1);

static void stop_motors(void)
{
	(void)motor_set_effort(MOTOR_LEFT_DEV, 0.0f);
	(void)motor_set_effort(MOTOR_RIGHT_DEV, 0.0f);
}

void effort_subscriber_task(void)
{
	const struct zbus_channel *chan;

	if (!device_is_ready(MOTOR_LEFT_DEV)) {
		LOG_ERR("left motor device not ready");
		return;
	}

	if (!device_is_ready(MOTOR_RIGHT_DEV)) {
		LOG_ERR("right motor device not ready");
		return;
	}

	LOG_INF("Starting subscriber on control channel");

	bool motors_stopped = true;

	while (true) {
		int rc = zbus_sub_wait(&effort_subscriber, &chan,
				       K_MSEC(CONFIG_KABOT_CONTROL_WATCHDOG_MS));

		if (!robot_settings_is_claimed()) {
			if (!motors_stopped) {
				LOG_INF("Control watchdog: unclaimed – stopping motors");
				stop_motors();
				motors_stopped = true;
			}
			continue;
		}

		if (rc != 0) {
			/* Watchdog fired: no command received within the window */
			if (!motors_stopped) {
				LOG_WRN("Control watchdog: no command in %d ms – stopping motors",
					CONFIG_KABOT_CONTROL_WATCHDOG_MS);
				stop_motors();
				motors_stopped = true;
			}
			continue;
		}

		if (&control_channel != chan) {
			continue;
		}

		Control control;
		if (zbus_chan_read(&control_channel, &control, K_MSEC(20)) == 0) {
			const float left_effort = control.effort.state.x;
			const float right_effort = control.effort.state.y;

			LOG_DBG("From subscriber -> Left effort=%.3f, Right effort=%.3f",
				(double)left_effort, (double)right_effort);

			int left_result = motor_set_effort(MOTOR_LEFT_DEV, left_effort);
			int right_result = motor_set_effort(MOTOR_RIGHT_DEV, right_effort);

			if (left_result < 0 || right_result < 0) {
				LOG_WRN("Failed to set motor effort: L=%d, R=%d", left_result,
					right_result);
			} else {
				motors_stopped = false;
			}
		} else {
			LOG_WRN("Failed to read from control_channel");
		}
	}
}

K_THREAD_DEFINE(effort_subscriber_task_id, CONFIG_MAIN_STACK_SIZE, effort_subscriber_task, NULL,
		NULL, NULL, 3, 0, 0);
