#include "zbus/state_channel.h"

#include <arpa/inet.h>
#include <errno.h>
#include <pb_encode.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/posix/sys/socket.h>
#include <zephyr/posix/unistd.h>

LOG_MODULE_REGISTER(state_udp_sender, LOG_LEVEL_DBG);

ZBUS_SUBSCRIBER_DEFINE(state_udp_sender, 4);

enum { ZBUS_READ_TIMEOUT_MS = 20 };

static int setup_state_socket(struct sockaddr_in *dest)
{
    int sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sock < 0) {
        return -errno;
    }

    dest->sin_family = AF_INET;
    dest->sin_port = htons(CONFIG_KABOT_STATE_EGRESS_PORT);

    int rc = inet_pton(AF_INET, CONFIG_KABOT_STATE_EGRESS_HOST, &dest->sin_addr);
    if (rc != 1) {
        close(sock);
        return -EINVAL;
    }

    return sock;
}

void state_udp_sender_task(void)
{
    const struct zbus_channel *chan;
    struct sockaddr_in dest = {0};
    int sock = setup_state_socket(&dest);

    if (sock < 0) {
        LOG_ERR("Failed to setup state UDP socket: %d", sock);
        return;
    }

    LOG_INF("State UDP sender active: %s:%d", CONFIG_KABOT_STATE_EGRESS_HOST,
            CONFIG_KABOT_STATE_EGRESS_PORT);

    while (!zbus_sub_wait(&state_udp_sender, &chan, K_FOREVER)) {
        if (&state_channel != chan) {
            continue;
        }

        State state;
        if (zbus_chan_read(&state_channel, &state, K_MSEC(ZBUS_READ_TIMEOUT_MS)) != 0) {
            LOG_WRN("Failed to read state_channel");
            continue;
        }

        uint8_t buf[512];
        pb_ostream_t stream = pb_ostream_from_buffer(buf, sizeof(buf));
        if (!pb_encode(&stream, State_fields, &state)) {
            LOG_WRN("Failed to encode State protobuf: %s", PB_GET_ERROR(&stream));
            continue;
        }

        ssize_t sent = sendto(sock, buf, stream.bytes_written, 0, (struct sockaddr *)&dest,
                              sizeof(dest));
        if (sent < 0) {
            LOG_WRN("State UDP send failed: %d", errno);
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
