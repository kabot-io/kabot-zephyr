#include "zbus/channels/state_channel.h"
#include "zbus/state_publish_utils.h"

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(sim_current_publisher, LOG_LEVEL_DBG);

void sim_current_publisher_task(void)
{
	LOG_INF("Sim current publisher active: %d ms", CONFIG_KABOT_STATE_CURRENT_PERIOD_MS);

	while (true) {
		const uint64_t stamp = state_now_stamp_ms();
		State state = State_init_zero;

		state.has_current_left = true;
		state.current_left.has_header = true;
		state.current_left.header.stamp = stamp;
		set_header_frame_id(&state.current_left.header,
				    CONFIG_KABOT_STATE_CURRENT_LEFT_FRAME_ID);
		state.current_left.state = random_rangef(0.0f, 0.5f);

		state.has_bus_voltage_left = true;
		state.bus_voltage_left.has_header = true;
		state.bus_voltage_left.header.stamp = stamp;
		set_header_frame_id(&state.bus_voltage_left.header,
				    CONFIG_KABOT_STATE_CURRENT_LEFT_FRAME_ID);
		state.bus_voltage_left.state = random_rangef(0.0f, 6.0f);

		state.has_power_left = true;
		state.power_left.has_header = true;
		state.power_left.header.stamp = stamp;
		set_header_frame_id(&state.power_left.header,
				    CONFIG_KABOT_STATE_CURRENT_LEFT_FRAME_ID);
		state.power_left.state = random_rangef(0.0f, 2.0f);

		state.has_current_right = true;
		state.current_right.has_header = true;
		state.current_right.header.stamp = stamp;
		set_header_frame_id(&state.current_right.header,
				    CONFIG_KABOT_STATE_CURRENT_RIGHT_FRAME_ID);
		state.current_right.state = random_rangef(0.0f, 0.5f);

		state.has_bus_voltage_right = true;
		state.bus_voltage_right.has_header = true;
		state.bus_voltage_right.header.stamp = stamp;
		set_header_frame_id(&state.bus_voltage_right.header,
				    CONFIG_KABOT_STATE_CURRENT_RIGHT_FRAME_ID);
		state.bus_voltage_right.state = random_rangef(0.0f, 6.0f);

		state.has_power_right = true;
		state.power_right.has_header = true;
		state.power_right.header.stamp = stamp;
		set_header_frame_id(&state.power_right.header,
				    CONFIG_KABOT_STATE_CURRENT_RIGHT_FRAME_ID);
		state.power_right.state = random_rangef(0.0f, 2.0f);

		state.has_current_supply = true;
		state.current_supply.has_header = true;
		state.current_supply.header.stamp = stamp;
		set_header_frame_id(&state.current_supply.header,
				    CONFIG_KABOT_STATE_CURRENT_SUPPLY_FRAME_ID);
		state.current_supply.state = random_rangef(0.0f, 0.5f);

		state.has_bus_voltage_supply = true;
		state.bus_voltage_supply.has_header = true;
		state.bus_voltage_supply.header.stamp = stamp;
		set_header_frame_id(&state.bus_voltage_supply.header,
				    CONFIG_KABOT_STATE_CURRENT_SUPPLY_FRAME_ID);
		state.bus_voltage_supply.state = random_rangef(0.0f, 6.0f);

		state.has_power_supply = true;
		state.power_supply.has_header = true;
		state.power_supply.header.stamp = stamp;
		set_header_frame_id(&state.power_supply.header,
				    CONFIG_KABOT_STATE_CURRENT_SUPPLY_FRAME_ID);
		state.power_supply.state = random_rangef(0.0f, 3.0f);

		int rc = publish_state_msg(&state, K_MSEC(CONFIG_KABOT_STATE_CURRENT_PERIOD_MS));
		if (rc != 0) {
			LOG_WRN("Failed to publish simulated current state: %d", rc);
		}

		k_sleep(K_MSEC(CONFIG_KABOT_STATE_CURRENT_PERIOD_MS));
	}
}

K_THREAD_DEFINE(sim_current_publisher_task_id, CONFIG_MAIN_STACK_SIZE, sim_current_publisher_task,
		NULL, NULL, NULL, 4, 0, 0);
