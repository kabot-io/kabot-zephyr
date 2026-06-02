#include "motor/motor_driver.h"
#include "motor/motor_math.h"

#include <errno.h>
#include <stdlib.h>
#include <zephyr/device.h>
#include <zephyr/shell/shell.h>

static int cmd_motor_list(const struct shell *sh, size_t argc, char **argv)
{
    ARG_UNUSED(argc);
    ARG_UNUSED(argv);

    size_t count = 0;

    for (size_t idx = 0;; idx++) {
        const struct device *dev = shell_device_lookup(idx, NULL);

        if (dev == NULL) {
            break;
        }

        if (!DEVICE_API_IS(motor, dev)) {
            continue;
        }

        if (count == 0) {
            shell_print(sh, "Registered motors:");
        }

        shell_print(sh, "  %s", dev->name);
        count++;
    }

    if (count == 0U) {
        shell_print(sh, "No motors registered.");
    }

    return 0;
}

static int cmd_motor_set(const struct shell *sh, size_t argc, char **argv)
{
    ARG_UNUSED(argc);

    const char *name = argv[1];
    char *end;

    errno = 0;
    float effort = strtof(argv[2], &end);

    if (errno == ERANGE || *end != '\0') {
        shell_error(sh, "effort must be a float in [-1.0, 1.0]");
        return -EINVAL;
    }

    if (!motor_effort_is_valid(effort)) {
        shell_error(sh, "effort must be a float in [-1.0, 1.0]");
        return -EINVAL;
    }

    const struct device *dev = shell_device_get_binding(name);
    if (dev == NULL || !DEVICE_API_IS(motor, dev)) {
        shell_error(sh, "motor device unknown (%s)", name);
        return -ENOENT;
    }

    int ret = motor_set_effort(dev, effort);

    if (ret < 0) {
        shell_error(sh, "set_effort failed: %d", ret);
        return ret;
    }

    shell_print(sh, "motor '%s' effort -> %.3f", name, effort);
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
                  "Usage: motor set <name> <effort>  (effort range: -1.0..1.0)",
                  cmd_motor_set, 3, 0),
    SHELL_SUBCMD_SET_END
);
/* clang-format on */

SHELL_CMD_REGISTER(motor, &motor_cmds, "Motor control commands.", NULL);
