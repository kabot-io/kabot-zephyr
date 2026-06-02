#include "motor/motor_service.h"
#include "protos/state_control_msg.pb.h"
#include "zbus/control_channel.h"

#include <arpa/inet.h>
#include <pb_decode.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/net/socket_service.h>
#include <zephyr/posix/poll.h>
#include <zephyr/posix/sys/socket.h>
#include <zephyr/posix/unistd.h>

LOG_MODULE_REGISTER(motor_service, LOG_LEVEL_DBG);

#define CONTROL_PORT 30010
#define MTU          1500

static int control_socket = -1;
static struct pollfd control_pollfd;

static void udp_motor_handler(struct net_socket_service_event *pev)
{
    struct pollfd *pfd = &pev->event;
    struct sockaddr_in sender_addr;
    socklen_t addrlen = sizeof(sender_addr);
    static char buf[MTU];

    int len = recvfrom(pfd->fd, buf, sizeof(buf), 0, (struct sockaddr *)&sender_addr, &addrlen);
    if (len < 0) {
        LOG_ERR("recvfrom failed with error %d", errno);
        return;
    }

    if (len == 0) {
        return;
    }

    Control msg = Control_init_zero;
    pb_istream_t stream = pb_istream_from_buffer((const pb_byte_t *)buf, (size_t)len);
    if (!pb_decode(&stream, Control_fields, &msg)) {
        LOG_WRN("Ignoring malformed control protobuf datagram (%d bytes)", len);
        return;
    }

    if (pfd->fd != control_socket) {
        LOG_ERR("Data received on unknown socket: %d", pfd->fd);
        return;
    }

    if (control_channel_validator(&msg, sizeof(msg))) {
        int publish_error = publish_control_msg(&msg, K_MSEC(100));
        if (publish_error) {
            LOG_ERR("Failed to publish control message: %d", publish_error);
        }
    }
}

NET_SOCKET_SERVICE_SYNC_DEFINE(udp_motor_service, udp_motor_handler, 1);

static int setup_socket(uint16_t port)
{
    int sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sock < 0) {
        return -errno;
    }

    struct sockaddr_in addr = {
        .sin_family = AF_INET,
        .sin_port = htons(port),
        .sin_addr.s_addr = INADDR_ANY,
    };

    if (bind(sock, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        close(sock);
        return -errno;
    }
    return sock;
}

void stop_motor_service(void)
{
    (void)net_socket_service_unregister(&udp_motor_service);
    if (control_socket >= 0) {
        close(control_socket);
        control_socket = -1;
    }
}

int start_motor_service(void)
{
    control_socket = setup_socket(CONTROL_PORT);
    if (control_socket < 0) {
        return control_socket;
    }

    control_pollfd.fd = control_socket;
    control_pollfd.events = POLLIN;

    int ret = net_socket_service_register(&udp_motor_service, &control_pollfd, 1, NULL);
    if (ret < 0) {
        stop_motor_service();
        return ret;
    }

    LOG_INF("Motor service active on port %d (protobuf Control)", CONTROL_PORT);
    return 0;
}
