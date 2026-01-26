#include <zephyr/init.h>
#include <zephyr/kernel.h>
#include <zephyr/zbus/zbus.h>

#include "motor_service.h"
#include "zbus/effort_msg.h"
#include "zbus/effort_channel.h"

#include <zephyr/net/wifi_mgmt.h>


#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(main, LOG_LEVEL_DBG);


ZBUS_SUBSCRIBER_DEFINE(effort_subscriber, 4);
static void effort_subscriber_task(void)
{
    const struct zbus_channel *chan;
    LOG_INF("Starting subscriber on effort channel");
    while (!zbus_sub_wait(&effort_subscriber, &chan, K_FOREVER)) {
        if (&effort_channel != chan) {
            continue;
        }
        struct effort_msg effort;
        if (zbus_chan_read(&effort_channel, &effort, K_MSEC(20)) == 0) {
            LOG_INF("From subscriber -> Left effort=%d, Right effort=%d", effort.left, effort.right);
        } else {
            LOG_WRN("Failed to read from effort_channel");
        }
    }
}
K_THREAD_DEFINE(effort_subscriber_task_id, CONFIG_MAIN_STACK_SIZE, effort_subscriber_task, NULL, NULL, NULL, 3, 0, 0);


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
    return start_motor_service();
}

SYS_INIT(kabot_init, APPLICATION, 99);

int main(void)
{
    // High-level application logic goes here
    while (1) {
        k_sleep(K_FOREVER);
    }
}
