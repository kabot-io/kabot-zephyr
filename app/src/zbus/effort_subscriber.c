
#include "zbus/effort_subscriber.h"
#include "zbus/effort_channel.h"

#include <zephyr/drivers/pwm.h>

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(effort_subscriber, LOG_LEVEL_DBG);

static const struct pwm_dt_spec left_motor_pwm = PWM_DT_SPEC_GET(DT_ALIAS(servo0));
static const struct pwm_dt_spec right_motor_pwm = PWM_DT_SPEC_GET(DT_ALIAS(servo1));

ZBUS_SUBSCRIBER_DEFINE(effort_subscriber, 1);
void effort_subscriber_task(void)
{
    const struct zbus_channel *chan;
    LOG_INF("Starting subscriber on effort channel");
    while (!zbus_sub_wait(&effort_subscriber, &chan, K_FOREVER)) {
        if (&effort_channel != chan) {
            continue;
        }
        struct effort_msg effort;
        if (zbus_chan_read(&effort_channel, &effort, K_MSEC(20)) == 0) {
            LOG_INF("From subscriber -> Left effort=%d, Right effort=%d", effort.left,
                    effort.right);
            set_motor_effort(&effort);
        } else {
            LOG_WRN("Failed to read from effort_channel");
        }
    }
}
K_THREAD_DEFINE(effort_subscriber_task_id, CONFIG_MAIN_STACK_SIZE, effort_subscriber_task, NULL,
                NULL, NULL, 3, 0, 0);
int initialize_motor_pwms(void)
{
    if (!device_is_ready(left_motor_pwm.dev)) {
        LOG_ERR("Left motor PWM device not ready");
        return -ENODEV;
    }
    if (!device_is_ready(right_motor_pwm.dev)) {
        LOG_ERR("Right motor PWM device not ready");
        return -ENODEV;
    }
    return 0;
}

uint32_t map_effort_to_pulse(int32_t effort, bool flip)
{
    // 1. Clamp input to ensure it's within [-100, 100]
    effort = CLAMP(effort, -100, 100);

    // 2. Apply flip if necessary
    if (flip) {
        effort = -effort;
    }

    // 3. Calculate: 1500 is the center, 5 is the scale factor
    // Result: -100 -> 1000, 0 -> 1500, 100 -> 2000
    int32_t pulse = 1500 + (effort * 5);

    return (uint32_t)pulse;
}

int set_motor_effort(struct effort_msg *effort)
{

    LOG_INF("Received motor efforts: Left=%d, Right=%d", effort->left, effort->right);

    uint32_t left_pulse = map_effort_to_pulse(effort->left, true);
    uint32_t right_pulse = map_effort_to_pulse(effort->right, false);

    LOG_INF("Setting motor efforts: Left pulse=%d us, Right pulse=%d us", left_pulse, right_pulse);

    int ret = pwm_set_dt(&left_motor_pwm, PWM_USEC(20000), PWM_USEC(left_pulse));
    if (ret < 0) {
        LOG_ERR("Failed to set left motor PWM: %d", ret);
        return ret;
    }

    ret = pwm_set_dt(&right_motor_pwm, PWM_USEC(20000), PWM_USEC(right_pulse));
    if (ret < 0) {
        LOG_ERR("Failed to set right motor PWM: %d", ret);
        return ret;
    }

    return 0;

    return 0;
}
