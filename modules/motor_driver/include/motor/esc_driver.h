#pragma once

#include <stdint.h>
#include <zephyr/drivers/pwm.h>

struct esc_motor_config {
	struct pwm_dt_spec pwm;
	uint32_t reverse_pulse;
	uint32_t stop_pulse;
	uint32_t forward_pulse;
};
