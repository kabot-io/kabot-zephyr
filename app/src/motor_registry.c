#include "motor_registry.h"

#include "esc_driver.h"
#include "sim_motor_driver.h"

#include <string.h>
#include <zephyr/devicetree.h>
#include <zephyr/sys/util.h>

#define MOTOR_LEFT_NODE  DT_ALIAS(motor_left)
#define MOTOR_RIGHT_NODE DT_ALIAS(motor_right)

#define MOTOR_NODE_IS_SUPPORTED(node_id)                                                           \
    (DT_NODE_HAS_COMPAT(node_id, kabot_esc) || DT_NODE_HAS_COMPAT(node_id, kabot_sim_motor))

BUILD_ASSERT(DT_NODE_EXISTS(MOTOR_LEFT_NODE),
             "Board overlay must provide a 'motor-left' alias for motor driver.");
BUILD_ASSERT(DT_NODE_EXISTS(MOTOR_RIGHT_NODE),
             "Board overlay must provide a 'motor-right' alias for motor driver.");

BUILD_ASSERT(MOTOR_NODE_IS_SUPPORTED(MOTOR_LEFT_NODE),
             "Alias 'motor-left' must target a node compatible with motor driver API.");
BUILD_ASSERT(MOTOR_NODE_IS_SUPPORTED(MOTOR_RIGHT_NODE),
             "Alias 'motor-right' must target a node compatible with motor driver API.");

#if DT_NODE_HAS_COMPAT(MOTOR_LEFT_NODE, kabot_esc)
static const struct esc_driver left_drv = ESC_DRIVER_FROM_DT(MOTOR_LEFT_NODE);
#elif DT_NODE_HAS_COMPAT(MOTOR_LEFT_NODE, kabot_sim_motor)
static const struct sim_motor_driver left_drv = SIM_MOTOR_DRIVER_DEFINE("left");
#else
#error "Unsupported compatible for 'motor-left' alias."
#endif

#if DT_NODE_HAS_COMPAT(MOTOR_RIGHT_NODE, kabot_esc)
static const struct esc_driver right_drv = ESC_DRIVER_FROM_DT(MOTOR_RIGHT_NODE);
#elif DT_NODE_HAS_COMPAT(MOTOR_RIGHT_NODE, kabot_sim_motor)
static const struct sim_motor_driver right_drv = SIM_MOTOR_DRIVER_DEFINE("right");
#else
#error "Unsupported compatible for 'motor-right' alias."
#endif

static const struct motor_registry_entry motors[] = {
    {.name = "left", .drv = (const struct motor_driver *)&left_drv},
    {.name = "right", .drv = (const struct motor_driver *)&right_drv},
};

size_t motor_registry_count(void)
{
    return ARRAY_SIZE(motors);
}

const struct motor_registry_entry *motor_registry_get(size_t idx)
{
    return idx < ARRAY_SIZE(motors) ? &motors[idx] : NULL;
}

const struct motor_registry_entry *motor_registry_find(const char *name)
{
    for (size_t i = 0; i < ARRAY_SIZE(motors); i++) {
        if (strcmp(motors[i].name, name) == 0) {
            return &motors[i];
        }
    }
    return NULL;
}
