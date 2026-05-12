#include <zephyr/init.h>
#include <zephyr/kernel.h>
#include <zephyr/zbus/zbus.h>

#include "motor/motor_service.h"
#include "zbus/effort_subscriber.h"
#include "zbus/effort_msg.h"
#include "zbus/effort_channel.h"
#include "zbus/sensor_subscriber.h"

#include <zephyr/net/wifi_mgmt.h>

#include <zephyr/logging/log.h>
#include <zephyr/logging/log_ctrl.h>

LOG_MODULE_REGISTER(main, LOG_LEVEL_DBG);

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

    autoconnect_wifi();
    initialize_motor_drivers();

    int rc = start_sensor_subscriber();
    if (rc < 0) {
        LOG_ERR("Failed to start sensor subscriber: %d", rc);
        return rc;
    }

    rc = start_motor_service();
    if (rc < 0) {
        LOG_ERR("Failed to start motor service: %d", rc);
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
