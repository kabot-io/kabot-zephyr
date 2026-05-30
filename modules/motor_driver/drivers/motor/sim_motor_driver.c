#include "motor/sim_motor_driver.h"

#include "motor/motor_driver.h"
#include "motor/motor_math.h"

#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(sim_motor, LOG_LEVEL_DBG);

#if DT_HAS_COMPAT_STATUS_OKAY(kabot_sim_motor)

static int sim_motor_init(const struct device *dev)
{
    const struct sim_motor_config *cfg = dev->config;

    LOG_DBG("sim motor '%s' ready (Gazebo bridge stub)", cfg->name);
    return 0;
}

static int sim_motor_set_effort(const struct device *dev, int32_t effort_q31)
{
    const struct sim_motor_config *cfg = dev->config;

    LOG_DBG("sim motor '%s' effort=%d%%", cfg->name, motor_q31_to_percent(effort_q31));
    return 0;
}

static DEVICE_API(motor, sim_motor_driver_api) = {
        .set_effort = sim_motor_set_effort,
};

#define SIM_MOTOR_DEFINE(node_id)                                                                  \
    static const struct sim_motor_config sim_motor_config_##node_id = {                            \
            .name = DT_NODE_FULL_NAME(node_id),                                                    \
    };                                                                                             \
    DEVICE_DT_DEFINE(node_id, sim_motor_init, NULL, NULL, &sim_motor_config_##node_id,             \
                     POST_KERNEL, CONFIG_MOTOR_DRIVER_INIT_PRIORITY, &sim_motor_driver_api)

DT_FOREACH_STATUS_OKAY(kabot_sim_motor, SIM_MOTOR_DEFINE);

#endif /* DT_HAS_COMPAT_STATUS_OKAY(kabot_sim_motor) */
