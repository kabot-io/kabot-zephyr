#include "zbus/state_egress_channel.h"

#include <arpa/inet.h>
#include <errno.h>
#include <pb_encode.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/posix/sys/socket.h>
#include <zephyr/posix/unistd.h>

#if defined(CONFIG_WIFI)
#include <zephyr/net/net_if.h>
#include <zephyr/net/net_ip.h>
#include <zephyr/net/wifi_mgmt.h>
#endif

LOG_MODULE_REGISTER(state_udp_sender, LOG_LEVEL_DBG);

ZBUS_SUBSCRIBER_DEFINE(state_udp_sender, 4);

enum { ZBUS_READ_TIMEOUT_MS = 20 };

static uint8_t state_encode_buf[512];

#if defined(CONFIG_WIFI)
static bool wifi_ready_for_state_egress(void)
{
    struct net_if *iface = net_if_get_wifi_sta();
    if (iface == NULL) {
        return false;
    }

    bool wifi_connected = net_if_is_up(iface);
    bool ipv4_ready = wifi_connected &&
                      (net_if_ipv4_get_global_addr(iface, NET_ADDR_PREFERRED) != NULL);

    return wifi_connected && ipv4_ready;
}
#endif

static int setup_state_socket(struct sockaddr_in *dest)
{
    int sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sock < 0) {
        return -errno;
    }

    dest->sin_family = AF_INET;
    dest->sin_port = htons(CONFIG_KABOT_STATE_EGRESS_PORT);

    int rc = inet_pton(AF_INET, CONFIG_KABOT_STATE_EGRESS_HOST, &dest->sin_addr);
    if (rc == 0) {
        close(sock);
        return -EINVAL;
    }
    if (rc < 0) {
        int err = errno;
        close(sock);
        return -err;
    }

    return sock;
}

void state_udp_sender_task(void)
{
    const struct zbus_channel *chan;
    struct sockaddr_in dest = {0};
#if defined(CONFIG_WIFI)
    bool network_ready = false;
    int64_t wait_start_ms = k_uptime_get();
    int64_t last_wait_log_ms = 0;
#endif

    int sock = setup_state_socket(&dest);

    if (sock < 0) {
        LOG_ERR("Failed to setup state UDP socket for %s:%d: %d",
                CONFIG_KABOT_STATE_EGRESS_HOST,
                CONFIG_KABOT_STATE_EGRESS_PORT,
                sock);
        return;
    }

    LOG_INF("State UDP sender active: %s:%d", CONFIG_KABOT_STATE_EGRESS_HOST,
            CONFIG_KABOT_STATE_EGRESS_PORT);

    while (!zbus_sub_wait(&state_udp_sender, &chan, K_FOREVER)) {
        if (&state_egress_channel != chan) {
            continue;
        }

        State state;
        if (zbus_chan_read(&state_egress_channel, &state, K_MSEC(ZBUS_READ_TIMEOUT_MS)) != 0) {
            LOG_WRN("Failed to read state_egress_channel");
            continue;
        }

#if defined(CONFIG_WIFI)
        if (!wifi_ready_for_state_egress()) {
            int64_t now_ms = k_uptime_get();
            int64_t waited_ms = now_ms - wait_start_ms;

            if (!network_ready) {
                if (last_wait_log_ms == 0 || (now_ms - last_wait_log_ms) >= 5000) {
                    LOG_INF("State UDP dropping messages: waiting for Wi-Fi/IP (%lld ms)",
                            waited_ms);
                    last_wait_log_ms = now_ms;
                }
            } else {
                LOG_WRN("State UDP network no longer ready; dropping until Wi-Fi/IP recovers");
                network_ready = false;
                wait_start_ms = now_ms;
                last_wait_log_ms = now_ms;
            }
            continue;
        }

        if (!network_ready) {
            int64_t ready_ms = k_uptime_get() - wait_start_ms;
            LOG_INF("State UDP network ready after %lld ms; sending resumed", ready_ms);
            network_ready = true;
        }
#endif

        pb_ostream_t stream = pb_ostream_from_buffer(state_encode_buf,
                                                     sizeof(state_encode_buf));
        if (!pb_encode(&stream, State_fields, &state)) {
            LOG_WRN("Failed to encode State protobuf: %s", PB_GET_ERROR(&stream));
            continue;
        }

        ssize_t sent = sendto(sock, state_encode_buf, stream.bytes_written, 0,
                              (struct sockaddr *)&dest,
                              sizeof(dest));
        if (sent < 0) {
            int err = errno;
            LOG_WRN("State UDP send failed: %d", err);
        } else {
            LOG_DBG("State UDP sent: %d bytes -> %s:%d", (int)sent,
                    CONFIG_KABOT_STATE_EGRESS_HOST,
                    CONFIG_KABOT_STATE_EGRESS_PORT);
        }
    }

    close(sock);
}

K_THREAD_DEFINE(state_udp_sender_task_id,
                CONFIG_MAIN_STACK_SIZE,
                state_udp_sender_task,
                NULL,
                NULL,
                NULL,
                4,
                0,
                0);
