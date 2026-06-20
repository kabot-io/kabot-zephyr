#include <zephyr/init.h>
#include <zephyr/kernel.h>
#include <zephyr/zbus/zbus.h>

#include "control/control_service.h"
#include "control/discovery_service.h"
#include "system/robot_settings.h"
#if defined(CONFIG_LED_STRIP)
#include "system/led_status_service.h"
#endif
#include "zbus/control/effort_subscriber.h"
#include "zbus/channels/control_channel.h"
#include "zbus/state/sensor_subscriber.h"

#include <zephyr/net/wifi_mgmt.h>

#include <zephyr/logging/log.h>
#include <zephyr/logging/log_ctrl.h>

LOG_MODULE_REGISTER(main, LOG_LEVEL_DBG);

#if defined(CONFIG_WIFI) && defined(CONFIG_LED_STRIP)
#define LED_STATUS_WIFI_EVENTS                                                                   \
	(NET_EVENT_WIFI_CONNECT_RESULT | NET_EVENT_WIFI_DISCONNECT_RESULT)

static struct net_mgmt_event_callback led_status_wifi_cb;

static void led_status_wifi_event_handler(struct net_mgmt_event_callback *cb, uint64_t mgmt_event,
						  struct net_if *iface)
{
	ARG_UNUSED(iface);

	const struct wifi_status *status = (const struct wifi_status *)cb->info;
	bool ok = status == NULL || status->status == 0;

	switch (mgmt_event) {
	case NET_EVENT_WIFI_CONNECT_RESULT:
		led_status_service_set_network_ready(ok);
		break;
	case NET_EVENT_WIFI_DISCONNECT_RESULT:
		if (ok) {
			led_status_service_set_network_ready(false);
		}
		break;
	default:
		break;
	}
}

static void register_led_status_wifi_events(void)
{
	net_mgmt_init_event_callback(&led_status_wifi_cb, led_status_wifi_event_handler,
					     LED_STATUS_WIFI_EVENTS);
	net_mgmt_add_event_callback(&led_status_wifi_cb);
}
#else
static void register_led_status_wifi_events(void)
{
}
#endif

static void init_led_status(void)
{
#if defined(CONFIG_LED_STRIP)
	int rc = led_status_service_init();
	if (rc < 0) {
		LOG_WRN("LED status service unavailable: %d", rc);
		return;
	}

	register_led_status_wifi_events();
#endif
}

#if defined(CONFIG_WIFI) && defined(CONFIG_WIFI_CREDENTIALS) &&                                    \
	defined(NET_REQUEST_WIFI_CONNECT_STORED)
static void autoconnect_wifi(void)
{
	LOG_INF("Auto-connecting to Wi-Fi...");

	struct net_if *iface = net_if_get_wifi_sta();

#ifdef CONFIG_WIFI_NM_WPA_SUPPLICANT_CRYPTO_ENTERPRISE
	wifi_set_enterprise_credentials(iface, 0);
#endif /* CONFIG_WIFI_NM_WPA_SUPPLICANT_CRYPTO_ENTERPRISE */

	int rc = net_mgmt(NET_REQUEST_WIFI_CONNECT_STORED, iface, NULL, 0);
	if (rc < 0) {
		LOG_ERR("Failed to connect to Wi-Fi: %d", rc);
	}
	// Implement Wi-Fi connection logic here
}
#else
static void autoconnect_wifi(void)
{
	LOG_INF("Wi-Fi autoconnect skipped: unsupported on this target");
}
#endif

// Automatic startup using Zephyr's init system
static int kabot_init(void)
{

	uint32_t domain_count = log_domains_count();
	for (uint32_t d = 0; d < domain_count; d++) {
		uint32_t source_count = log_src_cnt_get(d);
		for (uint32_t i = 0; i < source_count; i++) {
			log_filter_set(NULL, d, i, CONFIG_KABOT_LOG_LEVEL_DEFAULT);
		}
	}

	init_led_status();
	autoconnect_wifi();

	int rc = robot_settings_init();
	if (rc < 0) {
		LOG_ERR("Failed to initialize robot settings: %d", rc);
		return rc;
	}

	rc = start_sensor_subscriber();
	if (rc < 0) {
		LOG_ERR("Failed to start sensor subscriber: %d", rc);
		return rc;
	}

	rc = start_control_service();
	if (rc < 0) {
		LOG_ERR("Failed to start control service: %d", rc);
		stop_sensor_subscriber();
		return rc;
	}

	rc = start_discovery_service();
	if (rc < 0) {
		LOG_ERR("Failed to start discovery service: %d", rc);
		stop_control_service();
		stop_sensor_subscriber();
		return rc;
	}

	return 0;
}

SYS_INIT(kabot_init, APPLICATION, 99);

int main(void)
{
	// High-level application logic goes here
	LOG_INF("Main loop running");

	while (1) {
		k_sleep(K_FOREVER);
	}
}
