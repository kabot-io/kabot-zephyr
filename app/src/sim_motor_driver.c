#include "sim_motor_driver.h"

#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(sim_motor, LOG_LEVEL_DBG);

static int sim_motor_init(const struct motor_driver *drv)
{
    const struct sim_motor_driver *sim = (const struct sim_motor_driver *)drv;

    LOG_INF("sim motor '%s' ready (Gazebo bridge stub)", sim->name);
    return 0;
}

static int sim_motor_set_effort(const struct motor_driver *drv, int32_t effort)
{
    const struct sim_motor_driver *sim = (const struct sim_motor_driver *)drv;

    /* TODO: publish effort to Gazebo / physics simulator via ROS 2 bridge */
    LOG_DBG("sim motor '%s' effort=%d", sim->name, effort);
    return 0;
}

const struct motor_driver_api sim_motor_driver_api = {
    .init = sim_motor_init,
    .set_effort = sim_motor_set_effort,
};
