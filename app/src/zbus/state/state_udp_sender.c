#include "zbus/channels/state_egress_channel.h"
#include "system/robot_settings.h"

#include <arpa/inet.h>
#include <errno.h>
#include <pb_encode.h>
#include <stdio.h>
#include <string.h>
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

static int setup_state_socket(void)
{
    int sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sock < 0) {
        return -errno;
    }

    return sock;
}

static int apply_state_target(struct sockaddr_in *dest,
                              char *active_ip,
                              size_t active_ip_len,
                              uint16_t *active_port)
{
    char candidate_ip[KABOT_IPV4_STR_LEN] = {0};
    uint16_t candidate_port = 0;
    robot_settings_get_hmi_target(candidate_ip, sizeof(candidate_ip), &candidate_port);

    if ((candidate_port == 0U) || (candidate_ip[0] == '\0')) {
        LOG_WRN("State UDP target missing, keeping previous destination");
        return 0;
    }

    if ((strcmp(active_ip, candidate_ip) == 0) && (*active_port == candidate_port)) {
        return 0;
    }

    struct in_addr addr;
    int rc = inet_pton(AF_INET, candidate_ip, &addr);
    if (rc != 1) {
        LOG_WRN("State UDP target rejected, invalid IPv4: %s", candidate_ip);
        return -EINVAL;
    }

    dest->sin_family = AF_INET;
    dest->sin_port = htons(candidate_port);
    dest->sin_addr = addr;

    (void)snprintf(active_ip, active_ip_len, "%s", candidate_ip);
    *active_port = candidate_port;

    LOG_INF("State UDP target active: %s:%u", active_ip, (unsigned int)*active_port);
    return 0;
}

void state_udp_sender_task(void)
{
    const struct zbus_channel *chan;
    struct sockaddr_in dest = {0};
    char active_ip[KABOT_IPV4_STR_LEN] = {0};
    uint16_t active_port = 0;
    int64_t last_unclaimed_log_ms = 0;
#if defined(CONFIG_WIFI)
    bool network_ready = false;
    int64_t wait_start_ms = k_uptime_get();
    int64_t last_wait_log_ms = 0;
#endif

    int sock = setup_state_socket();

    if (sock < 0) {
        LOG_ERR("Failed to setup state UDP socket: %d", sock);
        return;
    }

    if (apply_state_target(&dest, active_ip, sizeof(active_ip), &active_port) < 0) {
        LOG_WRN("Failed to resolve initial state UDP target, waiting for claim");
    }

    while (!zbus_sub_wait(&state_udp_sender, &chan, K_FOREVER)) {
        if (&state_egress_channel != chan) {
            continue;
        }

        if (!robot_settings_is_claimed()) {
            int64_t now_ms = k_uptime_get();
            if ((last_unclaimed_log_ms == 0) || ((now_ms - last_unclaimed_log_ms) >= 5000)) {
                LOG_INF("State UDP waiting for claim; dropping egress until claim=true Bonjour arrives");
                last_unclaimed_log_ms = now_ms;
            }
            continue;
        }

        (void)apply_state_target(&dest, active_ip, sizeof(active_ip), &active_port);

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
            LOG_DBG("State UDP sent: %d bytes -> %s:%u", (int)sent, active_ip,
                    (unsigned int)active_port);
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
