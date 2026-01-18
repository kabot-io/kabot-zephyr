#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/net/socket_service.h>
#include <zephyr/posix/sys/socket.h>
#include <zephyr/posix/unistd.h>
#include <arpa/inet.h>

#include "motor_service.h"

LOG_MODULE_REGISTER(motor_service, LOG_LEVEL_DBG);

#define PORT_LEFT  30010
#define PORT_RIGHT 30020
#define MTU        1500

static int socket_left = -1;
static int socket_right = -1;
static struct pollfd sockfd_udp[2];

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
        /* Zero-length datagram received, do nothing. */
        return;
    }

    int8_t effort = (int8_t)buf[0];
    uint16_t port = (pfd->fd == socket_left) ? PORT_LEFT : PORT_RIGHT;
    LOG_INF("Port: %u | Effort: %d", port, effort);
}

NET_SOCKET_SERVICE_SYNC_DEFINE(udp_motor_service, udp_motor_handler, 2);

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
    if (socket_left >= 0) {
        close(socket_left);
        socket_left = -1;
    }
    if (socket_right >= 0) {
        close(socket_right);
        socket_right = -1;
    }
}

int start_motor_service(void)
{
    socket_left = setup_socket(PORT_LEFT);
    if (socket_left < 0) {
        return socket_left;
    }

    socket_right = setup_socket(PORT_RIGHT);
    if (socket_right < 0) {
        stop_motor_service(); // Cleanup resource 1
        return socket_right;
    }

    sockfd_udp[0].fd = socket_left;
    sockfd_udp[0].events = POLLIN;
    sockfd_udp[1].fd = socket_right;
    sockfd_udp[1].events = POLLIN;

    int ret = net_socket_service_register(&udp_motor_service, sockfd_udp, 2, NULL);
    if (ret < 0) {
        stop_motor_service();
        return ret;
    }

    LOG_INF("Motor service active on ports %d, %d", PORT_LEFT, PORT_RIGHT);
    return 0;
}
