/*
 * LED strip shell commands.
 *
 * Usage: led_strip set <R> <G> <B> [N]
 *   R/G/B - colour components 0..255
 *   N     - 0-based LED index (0 .. chain_length-1); omit to set all LEDs
 */

#include <errno.h>
#include <stdlib.h>
#include <string.h>

#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/led_strip.h>
#include <zephyr/kernel.h>
#include <zephyr/random/random.h>
#include <zephyr/shell/shell.h>

#define STRIP_NODE DT_NODELABEL(led_strip)

BUILD_ASSERT(DT_NODE_EXISTS(STRIP_NODE), "devicetree node 'led_strip' not found – check overlay");

#define STRIP_NUM_PIXELS DT_PROP(STRIP_NODE, chain_length)

static const struct device *const strip = DEVICE_DT_GET(STRIP_NODE);

/* Persistent pixel buffer – preserves previously set pixels. */
static struct led_rgb pixels[STRIP_NUM_PIXELS];

/* Helper: parse a long in [lo, hi] from a string. */
static int parse_long(const struct shell *sh, const char *str, long lo, long hi, long *out)
{
	char *end;

	errno = 0;
	long val = strtol(str, &end, 10);

	if (errno == ERANGE || end == str || *end != '\0') {
		shell_error(sh, "invalid number: %s", str);
		return -EINVAL;
	}
	if (val < lo || val > hi) {
		shell_error(sh, "%s out of range [%ld, %ld]", str, lo, hi);
		return -EINVAL;
	}

	*out = val;
	return 0;
}

static int cmd_led_strip_set(const struct shell *sh, size_t argc, char **argv)
{
	if (!device_is_ready(strip)) {
		shell_error(sh, "LED strip device not ready");
		return -ENODEV;
	}

	long r, g, b;
	int rc;

	rc = parse_long(sh, argv[1], 0, 255, &r);
	if (rc) {
		return rc;
	}
	rc = parse_long(sh, argv[2], 0, 255, &g);
	if (rc) {
		return rc;
	}
	rc = parse_long(sh, argv[3], 0, 255, &b);
	if (rc) {
		return rc;
	}

	struct led_rgb color = {.r = (uint8_t)r, .g = (uint8_t)g, .b = (uint8_t)b};

	if (argc == 5) {
		/* Single LED */
		long n;

		rc = parse_long(sh, argv[4], 0, STRIP_NUM_PIXELS - 1, &n);
		if (rc) {
			return rc;
		}
		pixels[n] = color;
		rc = led_strip_update_rgb(strip, pixels, STRIP_NUM_PIXELS);
		if (rc < 0) {
			shell_error(sh, "led_strip_update_rgb failed: %d", rc);
			return rc;
		}
		shell_print(sh, "led[%ld] = (#%02lX%02lX%02lX)", n, r, g, b);
	} else {
		/* All LEDs */
		for (size_t i = 0; i < STRIP_NUM_PIXELS; i++) {
			pixels[i] = color;
		}
		rc = led_strip_update_rgb(strip, pixels, STRIP_NUM_PIXELS);
		if (rc < 0) {
			shell_error(sh, "led_strip_update_rgb failed: %d", rc);
			return rc;
		}
		shell_print(sh, "all leds = (#%02lX%02lX%02lX)", r, g, b);
	}

	return 0;
}

static int cmd_led_strip_strobe(const struct shell *sh, size_t argc, char **argv)
{
	if (!device_is_ready(strip)) {
		shell_error(sh, "LED strip device not ready");
		return -ENODEV;
	}

	long pulse_ms, total_ms, count;
	int rc;

	rc = parse_long(sh, argv[1], 1, 10000, &pulse_ms);
	if (rc) {
		return rc;
	}
	rc = parse_long(sh, argv[2], 1, 600000, &total_ms);
	if (rc) {
		return rc;
	}
	rc = parse_long(sh, argv[3], 1, 10000, &count);
	if (rc) {
		return rc;
	}

	if (((int64_t)pulse_ms * (int64_t)count) > total_ms) {
		shell_error(sh, "pulse_ms * count must be <= total_ms");
		return -EINVAL;
	}

	long blink_r = 10;
	long blink_g = 10;
	long blink_b = 10;
	bool fixed_channel = false;
	long channel = -1;

	if (argc == 5) {
		rc = parse_long(sh, argv[4], 0, STRIP_NUM_PIXELS - 1, &channel);
		if (rc) {
			return rc;
		}
		fixed_channel = true;
	} else if (argc == 7) {
		rc = parse_long(sh, argv[4], 0, 255, &blink_r);
		if (rc) {
			return rc;
		}
		rc = parse_long(sh, argv[5], 0, 255, &blink_g);
		if (rc) {
			return rc;
		}
		rc = parse_long(sh, argv[6], 0, 255, &blink_b);
		if (rc) {
			return rc;
		}
	} else if (argc == 8) {
		rc = parse_long(sh, argv[4], 0, 255, &blink_r);
		if (rc) {
			return rc;
		}
		rc = parse_long(sh, argv[5], 0, 255, &blink_g);
		if (rc) {
			return rc;
		}
		rc = parse_long(sh, argv[6], 0, 255, &blink_b);
		if (rc) {
			return rc;
		}
		rc = parse_long(sh, argv[7], 0, STRIP_NUM_PIXELS - 1, &channel);
		if (rc) {
			return rc;
		}
		fixed_channel = true;
	} else if (argc != 4) {
		shell_error(sh, "Usage: led_strip strobe <pulse_ms> <total_ms> <count> [<R> <G> "
				"<B>] [<channel>]");
		return -EINVAL;
	}

	const struct led_rgb blink_color = {
		.r = (uint8_t)blink_r,
		.g = (uint8_t)blink_g,
		.b = (uint8_t)blink_b,
	};

	int64_t start_ms = k_uptime_get();

	for (long i = 0; i < count; i++) {
		int64_t pulse_start_ms = start_ms + ((int64_t)i * total_ms) / count;
		int64_t now_ms = k_uptime_get();
		if (now_ms < pulse_start_ms) {
			k_msleep((int32_t)(pulse_start_ms - now_ms));
		}

		long ch = channel;
		if (!fixed_channel) {
			ch = (long)(sys_rand32_get() % STRIP_NUM_PIXELS);
		}

		struct led_rgb previous = pixels[ch];
		pixels[ch] = blink_color;

		rc = led_strip_update_rgb(strip, pixels, STRIP_NUM_PIXELS);
		if (rc < 0) {
			shell_error(sh, "led_strip_update_rgb failed: %d", rc);
			return rc;
		}

		k_msleep((int32_t)pulse_ms);

		pixels[ch] = previous;

		rc = led_strip_update_rgb(strip, pixels, STRIP_NUM_PIXELS);
		if (rc < 0) {
			shell_error(sh, "led_strip_update_rgb failed: %d", rc);
			return rc;
		}
	}

	return 0;
}

/* clang-format off */
SHELL_STATIC_SUBCMD_SET_CREATE(led_strip_cmds,
    SHELL_CMD_ARG(set, NULL,
                  "Set LED(s) to an RGB colour.\n"
                  "Usage: led_strip set <R> <G> <B> [N]\n"
                  "  R G B  colour components 0..255\n"
                  "  N      0-based LED index (0..chain_length-1); omit to set all",
                  cmd_led_strip_set, 4, 1),
    SHELL_CMD_ARG(strobe, NULL,
                  "Overlay strobe pulses over current LED state.\n"
                  "Usage: led_strip strobe <pulse_ms> <total_ms> <count> [<R> <G> <B>] [<channel>]\n"
                  "  pulse_ms  pulse ON duration in ms\n"
                  "  total_ms  total burst duration in ms\n"
                  "  count     number of pulses\n"
                  "  R G B     optional blink color, default 10 10 10\n"
                  "  channel   optional LED index, random per pulse if omitted",
                  cmd_led_strip_strobe, 4, 4),
    SHELL_SUBCMD_SET_END
);
/* clang-format on */

SHELL_CMD_REGISTER(led_strip, &led_strip_cmds, "LED strip commands.", NULL);
