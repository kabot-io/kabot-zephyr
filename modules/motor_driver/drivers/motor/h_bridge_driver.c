#include "motor/h_bridge_driver.h"

#include "motor/motor_driver.h"
#include "motor/motor_math.h"

#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/util.h>

LOG_MODULE_REGISTER(h_bridge_driver, LOG_LEVEL_DBG);

#if DT_HAS_COMPAT_STATUS_OKAY(kabot_h_bridge)

static uint32_t effort_to_duty_ns(int32_t effort_q31, uint32_t period_ns)
{
    if (effort_q31 == 0) {
        return 0;
    }

    if (effort_q31 == MOTOR_EFFORT_Q31_MIN) {
        return period_ns;
    }

    int64_t magnitude_q31 = effort_q31 < 0 ? -(int64_t)effort_q31 : (int64_t)effort_q31;
    uint64_t scaled =
            ((uint64_t)magnitude_q31 * (uint64_t)period_ns) + ((uint64_t)MOTOR_EFFORT_Q31_MAX / 2U);
    uint32_t duty_ns = (uint32_t)(scaled / (uint64_t)MOTOR_EFFORT_Q31_MAX);

    return MIN(duty_ns, period_ns);
}

static int h_bridge_init(const struct device *dev)
{
    const struct h_bridge_motor_config *cfg = dev->config;

    if (!device_is_ready(cfg->pwm_a.dev) || !device_is_ready(cfg->pwm_b.dev)) {
        LOG_ERR("H-bridge PWM device not ready");
        return -ENODEV;
    }

    if (cfg->has_supply_gpio) {
        if (!device_is_ready(cfg->supply_gpio.port)) {
            LOG_ERR("H-bridge supply GPIO device not ready");
            return -ENODEV;
        }

        int ret = gpio_pin_configure_dt(&cfg->supply_gpio, GPIO_OUTPUT_ACTIVE);
        if (ret < 0) {
            LOG_ERR("Failed to configure H-bridge supply GPIO: %d", ret);
            return ret;
        }
    }

    return 0;
}

static int h_bridge_set_effort(const struct device *dev, int32_t effort_q31)
{
    const struct h_bridge_motor_config *cfg = dev->config;

    uint32_t duty_a_ns = 0;
    uint32_t duty_b_ns = 0;

    if (effort_q31 == 0) {
        /* Brake at zero effort by driving both inputs active. */
        duty_a_ns = cfg->pwm_a.period;
        duty_b_ns = cfg->pwm_b.period;
    } else if (effort_q31 > 0) {
        duty_a_ns = effort_to_duty_ns(effort_q31, cfg->pwm_a.period);
        duty_b_ns = 0;
    } else {
        duty_a_ns = 0;
        duty_b_ns = effort_to_duty_ns(effort_q31, cfg->pwm_b.period);
    }

    int ret = pwm_set_dt(&cfg->pwm_a, cfg->pwm_a.period, duty_a_ns);
    if (ret < 0) {
        LOG_ERR("Failed to set H-bridge PWM A: %d", ret);
        return ret;
    }

    ret = pwm_set_dt(&cfg->pwm_b, cfg->pwm_b.period, duty_b_ns);
    if (ret < 0) {
        LOG_ERR("Failed to set H-bridge PWM B: %d", ret);
        return ret;
    }

    return 0;
}

static DEVICE_API(motor, h_bridge_driver_api) = {
        .set_effort = h_bridge_set_effort,
};

#define H_BRIDGE_MOTOR_DEFINE(node_id)                                                             \
    static const struct h_bridge_motor_config h_bridge_config_##node_id = {                        \
            .pwm_a = PWM_DT_SPEC_GET_BY_IDX(node_id, 0),                                           \
            .pwm_b = PWM_DT_SPEC_GET_BY_IDX(node_id, 1),                                           \
            .supply_gpio = GPIO_DT_SPEC_GET_OR(node_id, supply_gpios, {0}),                        \
            .has_supply_gpio = DT_NODE_HAS_PROP(node_id, supply_gpios),                            \
    };                                                                                             \
    DEVICE_DT_DEFINE(node_id, h_bridge_init, NULL, NULL, &h_bridge_config_##node_id, POST_KERNEL,  \
                     CONFIG_PWM_INIT_PRIORITY, &h_bridge_driver_api)

DT_FOREACH_STATUS_OKAY(kabot_h_bridge, H_BRIDGE_MOTOR_DEFINE);

#endif /* DT_HAS_COMPAT_STATUS_OKAY(kabot_h_bridge) */
