
#include "zbus/effort_subscriber.h"
#include "zbus/effort_channel.h"
#include "motor_driver.h"
#include "motor_registry.h"

#include <string.h>
#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(effort_subscriber, LOG_LEVEL_DBG);

/*
 * Include the backend headers that are actually needed for this board.
 * Named macros make the intent explicit; each motor alias is checked
 * independently so boards where only one motor uses a given backend compile
 * cleanly.
 */
#define MOTOR_LEFT_IS_ESC  DT_NODE_HAS_COMPAT(DT_ALIAS(motor_left), kabot_esc)
#define MOTOR_RIGHT_IS_ESC DT_NODE_HAS_COMPAT(DT_ALIAS(motor_right), kabot_esc)

#if MOTOR_LEFT_IS_ESC || MOTOR_RIGHT_IS_ESC
#include "esc_driver.h"
#endif

/* Include sim header if at least one motor is not an ESC. */
#if !MOTOR_LEFT_IS_ESC || !MOTOR_RIGHT_IS_ESC
#include "sim_motor_driver.h"
#endif

/*
 * Instantiate the correct driver for each motor alias.
 *
 * The board overlay defines motor-left and motor-right aliases that point to
 * a hardware node (e.g. kabot,esc on ESP32S3) or a simulation node
 * (kabot,sim-motor on native_sim).  Adding a new backend (H-Bridge, …) only
 * requires a new #elif branch here — the subscriber and shell code are
 * untouched.
 */

/* --- motor-left ---------------------------------------------------------- */
#if MOTOR_LEFT_IS_ESC
static const struct esc_driver left_drv = ESC_DRIVER_FROM_DT(DT_ALIAS(motor_left));
#else
static const struct sim_motor_driver left_drv = SIM_MOTOR_DRIVER_DEFINE("left");
#endif

/* --- motor-right --------------------------------------------------------- */
#if MOTOR_RIGHT_IS_ESC
static const struct esc_driver right_drv = ESC_DRIVER_FROM_DT(DT_ALIAS(motor_right));
#else
static const struct sim_motor_driver right_drv = SIM_MOTOR_DRIVER_DEFINE("right");
#endif

/* --- Registry ------------------------------------------------------------ */

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

/* --- Subscriber task ----------------------------------------------------- */

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
            const struct motor_registry_entry *left = motor_registry_find("left");
            const struct motor_registry_entry *right = motor_registry_find("right");

            if (left != NULL) {
                (void)motor_driver_set_effort(left->drv, effort.left);
            }
            if (right != NULL) {
                (void)motor_driver_set_effort(right->drv, effort.right);
            }
        } else {
            LOG_WRN("Failed to read from effort_channel");
        }
    }
}
K_THREAD_DEFINE(effort_subscriber_task_id, CONFIG_MAIN_STACK_SIZE, effort_subscriber_task, NULL,
                NULL, NULL, 3, 0, 0);

/* --- Initialization ------------------------------------------------------ */

int initialize_motor_drivers(void)
{
    for (size_t i = 0; i < ARRAY_SIZE(motors); i++) {
        int ret = motor_driver_init(motors[i].drv);
        if (ret < 0) {
            LOG_ERR("Failed to init motor '%s': %d", motors[i].name, ret);
            return ret;
        }
    }
    return 0;
}
