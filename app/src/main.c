#include <zephyr/init.h>
#include <zephyr/kernel.h>
#include <zephyr/zbus/zbus.h>

#include "motor_service.h"
#include "zbus/effort_subscriber.h"
#include "zbus/effort_msg.h"
#include "zbus/effort_channel.h"

#include <zephyr/net/wifi_mgmt.h>


#include <zephyr/logging/log.h>
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
    autoconnect_wifi();
    initialize_motor_pwms();
    return start_motor_service();
}

SYS_INIT(kabot_init, APPLICATION, 99);

int main(void)
{
    // High-level application logic goes here
    LOG_INF("Main loop running");

    struct effort_msg forward = {
        .left = 100,
        .right = 100
    };

    struct effort_msg stop = {
        .left = 0,
        .right = 0
    };

    struct effort_msg backward = {
        .left = -100,
        .right = -100
    };

    while (1) {
        k_sleep(K_FOREVER);
        // set_motor_effort(&forward);
        // k_sleep(K_MSEC(3000));
        // set_motor_effort(&stop);
        // k_sleep(K_MSEC(500));
    }
}
