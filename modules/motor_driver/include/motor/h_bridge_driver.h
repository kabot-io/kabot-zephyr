#pragma once

#include <stdbool.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/pwm.h>

struct h_bridge_motor_config {
    struct pwm_dt_spec pwm_a;
    struct pwm_dt_spec pwm_b;
    struct gpio_dt_spec supply_gpio;
    bool has_supply_gpio;
};
