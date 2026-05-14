#include "motor/esc_driver.h"

#include "motor/motor_driver.h"
#include "motor/motor_math.h"

#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/util.h>

LOG_MODULE_REGISTER(esc_driver, LOG_LEVEL_DBG);

#if DT_HAS_COMPAT_STATUS_OKAY(kabot_esc)

static uint32_t lerp_pulse_q31(uint32_t start, uint32_t end, int64_t numerator, int64_t denominator)
{
    if (denominator <= 0) {
        return start;
    }

    int64_t delta = (int64_t)end - (int64_t)start;
    int64_t pulse = (int64_t)start + ((delta * numerator) / denominator);

    if (pulse < 0) {
        return 0;
    }

    return (uint32_t)pulse;
}

static uint32_t map_effort_to_pulse(int32_t effort_q31, uint32_t reverse_pulse, uint32_t stop_pulse,
                                    uint32_t forward_pulse)
{
    if (effort_q31 <= 0) {
        int64_t numerator = (int64_t)effort_q31 - (int64_t)MOTOR_EFFORT_Q31_MIN;
        return lerp_pulse_q31(reverse_pulse, stop_pulse, numerator, 2147483648LL);
    }

    return lerp_pulse_q31(stop_pulse, forward_pulse, (int64_t)effort_q31,
                          (int64_t)MOTOR_EFFORT_Q31_MAX);
}

static int esc_init(const struct device *dev)
{
    const struct esc_motor_config *cfg = dev->config;

    if (!device_is_ready(cfg->pwm.dev)) {
        LOG_ERR("ESC PWM device not ready");
        return -ENODEV;
    }

    return 0;
}

static int esc_set_effort(const struct device *dev, int32_t effort_q31)
{
    const struct esc_motor_config *cfg = dev->config;
    uint32_t pulse = map_effort_to_pulse(effort_q31, cfg->reverse_pulse, cfg->stop_pulse,
                                         cfg->forward_pulse);

    int ret = pwm_set_dt(&cfg->pwm, cfg->pwm.period, pulse);
    if (ret < 0) {
        LOG_ERR("Failed to set ESC PWM: %d", ret);
        return ret;
    }

    return 0;
}

static DEVICE_API(motor, esc_driver_api) = {
    .set_effort = esc_set_effort,
};

#define ESC_MOTOR_DEFINE(node_id)                                                                 \
    static const struct esc_motor_config esc_config_##node_id = {                                \
        .pwm = PWM_DT_SPEC_GET(node_id),                                                          \
        .reverse_pulse = DT_PROP(node_id, reverse_pulse),                                         \
        .stop_pulse = DT_PROP(node_id, stop_pulse),                                               \
        .forward_pulse = DT_PROP(node_id, forward_pulse),                                         \
    };                                                                                             \
    DEVICE_DT_DEFINE(node_id, esc_init, NULL, NULL, &esc_config_##node_id, POST_KERNEL,          \
                     CONFIG_PWM_INIT_PRIORITY, &esc_driver_api)

DT_FOREACH_STATUS_OKAY(kabot_esc, ESC_MOTOR_DEFINE);

#endif /* DT_HAS_COMPAT_STATUS_OKAY(kabot_esc) */
