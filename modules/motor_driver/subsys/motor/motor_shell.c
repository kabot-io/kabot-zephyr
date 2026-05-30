#include "motor/motor_driver.h"
#include "motor/motor_math.h"

#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/shell/shell.h>

#define MOTOR_NAME_ENTRY(node_id) DT_NODE_FULL_NAME(node_id),

static const char *const motor_names[] = {
        DT_FOREACH_STATUS_OKAY(kabot_esc, MOTOR_NAME_ENTRY)
                DT_FOREACH_STATUS_OKAY(kabot_h_bridge, MOTOR_NAME_ENTRY)
                        DT_FOREACH_STATUS_OKAY(kabot_sim_motor, MOTOR_NAME_ENTRY)};

static bool motor_name_is_known(const char *name)
{
    if (name == NULL) {
        return false;
    }

    for (size_t i = 0; i < ARRAY_SIZE(motor_names); i++) {
        if (strcmp(motor_names[i], name) == 0) {
            return true;
        }
    }

    return false;
}

static int cmd_motor_list(const struct shell *sh, size_t argc, char **argv)
{
    ARG_UNUSED(argc);
    ARG_UNUSED(argv);

    if (ARRAY_SIZE(motor_names) == 0) {
        shell_print(sh, "No motors registered.");
        return 0;
    }

    shell_print(sh, "Registered motors:");
    for (size_t i = 0; i < ARRAY_SIZE(motor_names); i++) {
        shell_print(sh, "  %s", motor_names[i]);
    }

    return 0;
}

static int cmd_motor_set(const struct shell *sh, size_t argc, char **argv)
{
    ARG_UNUSED(argc);

    const char *name = argv[1];
    char *end;

    errno = 0;
    long effort = strtol(argv[2], &end, 10);

    if (errno == ERANGE || *end != '\0') {
        shell_error(sh, "effort must be an integer in [-100, 100]");
        return -EINVAL;
    }

    int32_t effort_q31;
    int convert_error = motor_percent_to_q31((int32_t)effort, &effort_q31);
    if (convert_error < 0) {
        shell_error(sh, "effort must be an integer in [-100, 100]");
        return convert_error;
    }

    if (!motor_name_is_known(name)) {
        shell_error(sh, "motor device unknown (%s)", name);
        return -ENOENT;
    }

    const struct device *dev = shell_device_get_binding(name);
    if (dev == NULL) {
        shell_error(sh, "motor device unknown (%s)", name);
        return -ENOENT;
    }

    int ret = motor_set_effort(dev, effort_q31);

    if (ret < 0) {
        shell_error(sh, "set_effort failed: %d", ret);
        return ret;
    }

    shell_print(sh, "motor '%s' effort -> %ld", name, effort);
    return 0;
}

/* clang-format off */
SHELL_STATIC_SUBCMD_SET_CREATE(motor_cmds,
    SHELL_CMD_ARG(list, NULL,
                  "List all registered motors.\n"
                  "Usage: motor list",
                  cmd_motor_list, 1, 0),
    SHELL_CMD_ARG(set, NULL,
                  "Set motor effort.\n"
                  "Usage: motor set <name> <effort>  (effort range: -100..100)",
                  cmd_motor_set, 3, 0),
    SHELL_SUBCMD_SET_END
);
/* clang-format on */

SHELL_CMD_REGISTER(motor, &motor_cmds, "Motor control commands.", NULL);
