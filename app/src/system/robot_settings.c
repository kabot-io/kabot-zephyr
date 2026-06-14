#include "system/robot_settings.h"

#include <arpa/inet.h>
#include <errno.h>
#include <string.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/settings/settings.h>

LOG_MODULE_REGISTER(robot_settings, LOG_LEVEL_DBG);

static K_MUTEX_DEFINE(robot_settings_lock);

static char g_serial[KABOT_SERIAL_MAX_LEN];
static char g_human_name[KABOT_HUMAN_NAME_MAX_LEN];
static char g_hmi_ip[KABOT_IPV4_STR_LEN];
static uint16_t g_hmi_port;
static bool g_claimed;
static char g_claimed_by_ip[KABOT_IPV4_STR_LEN];

#if defined(CONFIG_SETTINGS)
static bool g_registered;
#endif

static void copy_cstr(char *dst, size_t dst_len, const char *src)
{
	if ((dst == NULL) || (dst_len == 0U)) {
		return;
	}

	if (src == NULL) {
		dst[0] = '\0';
		return;
	}

	size_t src_len = strlen(src);
	size_t copy_len = (src_len < (dst_len - 1U)) ? src_len : (dst_len - 1U);
	memcpy(dst, src, copy_len);
	dst[copy_len] = '\0';
}

static void load_defaults_locked(void)
{
	copy_cstr(g_serial, sizeof(g_serial), CONFIG_KABOT_DEFAULT_SERIAL);
	copy_cstr(g_human_name, sizeof(g_human_name), CONFIG_KABOT_DEFAULT_HUMAN_NAME);
	copy_cstr(g_hmi_ip, sizeof(g_hmi_ip), CONFIG_KABOT_STATE_EGRESS_HOST);
	g_hmi_port = (uint16_t)CONFIG_KABOT_STATE_EGRESS_PORT;
	g_claimed = false;
	g_claimed_by_ip[0] = '\0';
}

#if defined(CONFIG_SETTINGS)
static int read_string_value(settings_read_cb read_cb, void *cb_arg, size_t len_rd, char *dst,
			     size_t dst_size)
{
	if ((dst == NULL) || (dst_size == 0U)) {
		return -EINVAL;
	}

	size_t to_read = (len_rd < (dst_size - 1U)) ? len_rd : (dst_size - 1U);
	ssize_t rc = read_cb(cb_arg, dst, to_read);
	if (rc < 0) {
		return (int)rc;
	}

	dst[to_read] = '\0';

	if ((size_t)rc < to_read) {
		dst[rc] = '\0';
	}

	return 0;
}

static int read_u16_value(settings_read_cb read_cb, void *cb_arg, size_t len_rd, uint16_t *out)
{
	if (len_rd != sizeof(uint16_t)) {
		return -EINVAL;
	}

	ssize_t rc = read_cb(cb_arg, out, sizeof(uint16_t));
	if (rc < 0) {
		return (int)rc;
	}

	if (rc != sizeof(uint16_t)) {
		return -EINVAL;
	}

	return 0;
}

static int robot_settings_handler_set(const char *name, size_t len_rd, settings_read_cb read_cb,
				      void *cb_arg)
{
	const char *next = NULL;

	if (settings_name_steq(name, "id/serial", &next) && (next != NULL) && (*next == '\0')) {
		char value[KABOT_SERIAL_MAX_LEN] = {0};
		int rc = read_string_value(read_cb, cb_arg, len_rd, value, sizeof(value));
		if (rc < 0) {
			return rc;
		}

		k_mutex_lock(&robot_settings_lock, K_FOREVER);
		copy_cstr(g_serial, sizeof(g_serial), value);
		k_mutex_unlock(&robot_settings_lock);
		return 0;
	}

	if (settings_name_steq(name, "id/human_name", &next) && (next != NULL) && (*next == '\0')) {
		char value[KABOT_HUMAN_NAME_MAX_LEN] = {0};
		int rc = read_string_value(read_cb, cb_arg, len_rd, value, sizeof(value));
		if (rc < 0) {
			return rc;
		}

		k_mutex_lock(&robot_settings_lock, K_FOREVER);
		copy_cstr(g_human_name, sizeof(g_human_name), value);
		k_mutex_unlock(&robot_settings_lock);
		return 0;
	}

	if (settings_name_steq(name, "net/hmi_ip", &next) && (next != NULL) && (*next == '\0')) {
		char value[KABOT_IPV4_STR_LEN] = {0};
		int rc = read_string_value(read_cb, cb_arg, len_rd, value, sizeof(value));
		if (rc < 0) {
			return rc;
		}

		struct in_addr dummy;
		if (inet_pton(AF_INET, value, &dummy) != 1) {
			return -EINVAL;
		}

		k_mutex_lock(&robot_settings_lock, K_FOREVER);
		copy_cstr(g_hmi_ip, sizeof(g_hmi_ip), value);
		k_mutex_unlock(&robot_settings_lock);
		return 0;
	}

	if (settings_name_steq(name, "net/hmi_port", &next) && (next != NULL) && (*next == '\0')) {
		uint16_t value = 0;
		int rc = read_u16_value(read_cb, cb_arg, len_rd, &value);
		if (rc < 0) {
			return rc;
		}

		if (value == 0U) {
			return -EINVAL;
		}

		k_mutex_lock(&robot_settings_lock, K_FOREVER);
		g_hmi_port = value;
		k_mutex_unlock(&robot_settings_lock);
		return 0;
	}

	return -ENOENT;
}

static int robot_settings_handler_export(int (*cb)(const char *name, const void *value,
						   size_t val_len))
{
	char serial[KABOT_SERIAL_MAX_LEN] = {0};
	char human_name[KABOT_HUMAN_NAME_MAX_LEN] = {0};
	char hmi_ip[KABOT_IPV4_STR_LEN] = {0};
	uint16_t hmi_port = 0;

	k_mutex_lock(&robot_settings_lock, K_FOREVER);
	copy_cstr(serial, sizeof(serial), g_serial);
	copy_cstr(human_name, sizeof(human_name), g_human_name);
	copy_cstr(hmi_ip, sizeof(hmi_ip), g_hmi_ip);
	hmi_port = g_hmi_port;
	k_mutex_unlock(&robot_settings_lock);

	int rc = cb("kabot/id/serial", serial, strlen(serial));
	if (rc < 0) {
		return rc;
	}

	rc = cb("kabot/id/human_name", human_name, strlen(human_name));
	if (rc < 0) {
		return rc;
	}

	rc = cb("kabot/net/hmi_ip", hmi_ip, strlen(hmi_ip));
	if (rc < 0) {
		return rc;
	}

	return cb("kabot/net/hmi_port", &hmi_port, sizeof(hmi_port));
}

SETTINGS_STATIC_HANDLER_DEFINE(kabot_settings, "kabot", NULL, robot_settings_handler_set, NULL,
			       robot_settings_handler_export);
#endif

int robot_settings_init(void)
{
	k_mutex_lock(&robot_settings_lock, K_FOREVER);
	load_defaults_locked();
	k_mutex_unlock(&robot_settings_lock);

#if !defined(CONFIG_SETTINGS)
	LOG_INF("Robot settings backend disabled, using Kconfig defaults only");
	return 0;
#else
	if (!g_registered) {
		int rc = settings_subsys_init();
		if ((rc < 0) && (rc != -EALREADY)) {
			LOG_ERR("settings_subsys_init failed: %d", rc);
			return rc;
		}

		g_registered = true;
	}

	int rc = settings_load_subtree("kabot");
	if (rc < 0) {
		LOG_ERR("settings_load_subtree(kabot) failed: %d", rc);
		return rc;
	}

	char ip[KABOT_IPV4_STR_LEN] = {0};
	uint16_t port = 0;
	char serial[KABOT_SERIAL_MAX_LEN] = {0};
	char human_name[KABOT_HUMAN_NAME_MAX_LEN] = {0};

	robot_settings_get_hmi_target(ip, sizeof(ip), &port);
	robot_settings_get_identity(serial, sizeof(serial), human_name, sizeof(human_name));

	LOG_INF("Robot settings ready: serial=%s name=%s hmi=%s:%u", serial, human_name, ip,
		(unsigned int)port);
	return 0;
#endif
}

void robot_settings_get_identity(char *serial_out, size_t serial_out_len, char *human_name_out,
				 size_t human_name_out_len)
{
	k_mutex_lock(&robot_settings_lock, K_FOREVER);
	copy_cstr(serial_out, serial_out_len, g_serial);
	copy_cstr(human_name_out, human_name_out_len, g_human_name);
	k_mutex_unlock(&robot_settings_lock);
}

void robot_settings_get_hmi_target(char *ip_out, size_t ip_out_len, uint16_t *port_out)
{
	k_mutex_lock(&robot_settings_lock, K_FOREVER);
	copy_cstr(ip_out, ip_out_len, g_hmi_ip);
	if (port_out != NULL) {
		*port_out = g_hmi_port;
	}
	k_mutex_unlock(&robot_settings_lock);
}

int robot_settings_set_hmi_target(const char *ip, uint16_t port)
{
	if ((ip == NULL) || (port == 0U)) {
		return -EINVAL;
	}

	struct in_addr dummy;
	if (inet_pton(AF_INET, ip, &dummy) != 1) {
		return -EINVAL;
	}

	char ip_to_save[KABOT_IPV4_STR_LEN] = {0};
	copy_cstr(ip_to_save, sizeof(ip_to_save), ip);

	k_mutex_lock(&robot_settings_lock, K_FOREVER);
	copy_cstr(g_hmi_ip, sizeof(g_hmi_ip), ip_to_save);
	g_hmi_port = port;
	g_claimed = true;
	copy_cstr(g_claimed_by_ip, sizeof(g_claimed_by_ip), ip_to_save);
	k_mutex_unlock(&robot_settings_lock);

#if !defined(CONFIG_SETTINGS)
	return 0;
#else
	int rc = settings_save_one("kabot/net/hmi_ip", ip_to_save, strlen(ip_to_save));
	if (rc < 0) {
		return rc;
	}

	return settings_save_one("kabot/net/hmi_port", &port, sizeof(port));
#endif
}

bool robot_settings_is_claimed(void)
{
	bool claimed;

	k_mutex_lock(&robot_settings_lock, K_FOREVER);
	claimed = g_claimed;
	k_mutex_unlock(&robot_settings_lock);

	return claimed;
}

void robot_settings_clear_claim(void)
{
	k_mutex_lock(&robot_settings_lock, K_FOREVER);
	g_claimed = false;
	g_claimed_by_ip[0] = '\0';
	k_mutex_unlock(&robot_settings_lock);
}

void robot_settings_get_claim_state(bool *claimed_out, char *claimed_by_ip_out,
				    size_t claimed_by_ip_out_len)
{
	k_mutex_lock(&robot_settings_lock, K_FOREVER);
	if (claimed_out != NULL) {
		*claimed_out = g_claimed;
	}
	copy_cstr(claimed_by_ip_out, claimed_by_ip_out_len, g_claimed_by_ip);
	k_mutex_unlock(&robot_settings_lock);
}
