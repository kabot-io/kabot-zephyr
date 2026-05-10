#pragma once

#include <stdint.h>
#include <zephyr/sys/__assert.h>

/**
 * @brief Abstract motor driver interface.
 *
 * Every concrete motor backend (ESC, H-Bridge, simulation stub, ...) embeds
 * a @ref motor_driver as its **first** member and fills in the @p api
 * pointer.  Callers interact exclusively through the inline helpers below
 * so that the subscriber code never needs to know which backend is in use.
 */

struct motor_driver;

/** Function pointer table that every motor driver backend must implement. */
struct motor_driver_api {
    /**
     * @brief Initialize the motor hardware.
     * @return 0 on success, negative errno on failure.
     */
    int (*init)(const struct motor_driver *drv);

    /**
     * @brief Set the motor effort.
     * @param drv    Motor driver instance.
     * @param effort Desired effort in percent, range [-100, 100].
     *               Positive values drive forward, negative values in reverse.
     * @return 0 on success, negative errno on failure.
     */
    int (*set_effort)(const struct motor_driver *drv, int32_t effort);
};

/** Base struct embedded as the first member of every concrete motor driver. */
struct motor_driver {
    const struct motor_driver_api *api;
};

/** Initialize the motor driver hardware. */
static inline int motor_driver_init(const struct motor_driver *drv)
{
    __ASSERT(drv != NULL && drv->api != NULL && drv->api->init != NULL,
             "motor_driver: invalid driver instance");
    return drv->api->init(drv);
}

/** Set motor effort in percent [-100, 100]. */
static inline int motor_driver_set_effort(const struct motor_driver *drv, int32_t effort)
{
    __ASSERT(drv != NULL && drv->api != NULL && drv->api->set_effort != NULL,
             "motor_driver: invalid driver instance");
    return drv->api->set_effort(drv, effort);
}
