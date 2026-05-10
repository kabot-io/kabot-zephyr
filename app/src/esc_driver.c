#include "esc_driver.h"

#include <zephyr/logging/log.h>
#include <zephyr/sys/util.h>

LOG_MODULE_REGISTER(esc_driver, LOG_LEVEL_DBG);

static uint32_t lerp_pulse(uint32_t start, uint32_t end, int32_t percent)
{
    int32_t delta = (int32_t)end - (int32_t)start;
    int32_t pulse = (int32_t)start + ((delta * percent) / 100);

    return (uint32_t)pulse;
}

static uint32_t map_effort_to_pulse(int32_t effort, uint32_t reverse_pulse, uint32_t stop_pulse,
                                    uint32_t forward_pulse, bool flip)
{
    effort = CLAMP(effort, -100, 100);
    if (flip) {
        effort = -effort;
    }

    if (effort <= 0) {
        return lerp_pulse(reverse_pulse, stop_pulse, effort + 100);
    }

    return lerp_pulse(stop_pulse, forward_pulse, effort);
}

static int esc_init(const struct motor_driver *drv)
{
    const struct esc_driver *esc = (const struct esc_driver *)drv;

    if (!device_is_ready(esc->pwm.dev)) {
        LOG_ERR("ESC PWM device not ready");
        return -ENODEV;
    }

    return 0;
}

static int esc_set_effort(const struct motor_driver *drv, int32_t effort)
{
    const struct esc_driver *esc = (const struct esc_driver *)drv;
    uint32_t pulse = map_effort_to_pulse(effort, esc->reverse_pulse, esc->stop_pulse,
                                         esc->forward_pulse, esc->flip);

    int ret = pwm_set_dt(&esc->pwm, esc->pwm.period, pulse);
    if (ret < 0) {
        LOG_ERR("Failed to set ESC PWM: %d", ret);
        return ret;
    }

    return 0;
}

const struct motor_driver_api esc_driver_api = {
    .init = esc_init,
    .set_effort = esc_set_effort,
};
