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

    const struct device *dev = shell_device_get_binding(name);
    if (dev == NULL || !DEVICE_API_IS(motor, dev)) {
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
