#include "motor_registry.h"

#include <errno.h>
#include <stdlib.h>
#include <zephyr/shell/shell.h>

static int cmd_motor_list(const struct shell *sh, size_t argc, char **argv)
{
    ARG_UNUSED(argc);
    ARG_UNUSED(argv);

    size_t count = motor_registry_count();

    if (count == 0) {
        shell_print(sh, "No motors registered.");
        return 0;
    }

    shell_print(sh, "Registered motors:");
    for (size_t i = 0; i < count; i++) {
        const struct motor_registry_entry *e = motor_registry_get(i);

        shell_print(sh, "  %s", e->name);
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

    if (errno == ERANGE || *end != '\0' || effort < -100 || effort > 100) {
        shell_error(sh, "effort must be an integer in [-100, 100]");
        return -EINVAL;
    }

    const struct motor_registry_entry *e = motor_registry_find(name);

    if (e == NULL) {
        shell_error(sh, "motor '%s' not found", name);
        return -ENOENT;
    }

    int ret = motor_driver_set_effort(e->drv, (int32_t)effort);

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
