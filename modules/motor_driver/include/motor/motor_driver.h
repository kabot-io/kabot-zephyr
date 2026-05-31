#pragma once

#include "motor/motor_math.h"

#include <errno.h>
#include <stdint.h>
#include <zephyr/device.h>
#include <zephyr/sys/iterable_sections.h>
#include <zephyr/sys/__assert.h>

/**
 * @brief Zephyr motor driver subsystem interface.
 *
 * Concrete backends register device instances with @ref DEVICE_DT_DEFINE and
 * provide this API through @ref DEVICE_API_GET(motor, dev).
 */

typedef int (*motor_set_effort_t)(const struct device *dev, float effort);

/** Function pointer table that every motor driver backend must implement. */
__subsystem struct motor_driver_api {

    /**
     * @brief Set the motor effort.
     * @param dev Motor device instance.
     * @param effort Desired normalized effort in range [-1.0, 1.0].
     *               Positive values drive forward, negative values in reverse.
     * @return 0 on success, negative errno on failure.
     */
    motor_set_effort_t set_effort;
};

/** Set normalized motor effort in range [-1.0, 1.0]. */
__syscall int motor_set_effort(const struct device *dev, float effort);

static inline int z_impl_motor_set_effort(const struct device *dev, float effort)
{
    __ASSERT(dev != NULL, "motor: invalid device");

    const struct motor_driver_api *api = DEVICE_API_GET(motor, dev);

    __ASSERT(api != NULL && api->set_effort != NULL, "motor: missing set_effort API");

    if (!motor_effort_is_valid(effort)) {
        return -EINVAL;
    }

    return api->set_effort(dev, effort);
}

#include <zephyr/syscalls/motor_driver.h>
