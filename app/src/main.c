#include "zephyr/init.h"
#include "zephyr/net/net_ip.h"
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include <zephyr/posix/sys/socket.h>
#include <zephyr/posix/unistd.h>
#include <zephyr/net/socket_service.h>
#include <zephyr/posix/poll.h>
#include <arpa/inet.h>
#include <stdint.h>

LOG_MODULE_REGISTER(kabot, LOG_LEVEL_DBG);

#define PORT_LEFT  30010
#define PORT_RIGHT 30020
#define MTU        1500

static int socket_left = -1;
static int socket_right = -1;
static struct pollfd sockfd_udp[2];

static void udp_motor_service_handler(struct net_socket_service_event *pev)
{
    struct pollfd *pfd = &pev->event;
    int client_socket = pfd->fd;
    struct sockaddr_in sender_addr;
    socklen_t addrlen = sizeof(sender_addr);

    static char buf[MTU];
    int len;

    len = recvfrom(client_socket, buf, sizeof(buf), 0, (struct sockaddr *)&sender_addr, &addrlen);

    if (len <= 0) {
        return;
    }

    // 1. Identify Port
    uint16_t local_port = (client_socket == socket_left) ? PORT_LEFT : PORT_RIGHT;

    if (len != 1) {
        LOG_ERR("Port: %u | Error: Expected 1 byte, but got %d", local_port, len);
        return;
    }

    // 3. Cast to signed 8-bit integer (Matches Python's 'b' format)
    int8_t motor_effort = (int8_t)buf[0];

    // 4. Log as a signed integer
    LOG_INF("Received motor effort command:");
    LOG_INF("Port: %u | Effort: %d", local_port, motor_effort);
}

/* --- Rest of the setup remains the same --- */

NET_SOCKET_SERVICE_SYNC_DEFINE(udp_motor_service, udp_motor_service_handler, 2);

int setup_socket_and_bind(struct sockaddr_in *addr)
{
    int sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sock < 0) {
        return -errno;
    }
    if (bind(sock, (struct sockaddr *)addr, sizeof(*addr)) < 0) {
        close(sock);
        return -errno;
    }
    return sock;
}

static void close_motor_sockets(void)
{
    /* 1. Unregister the socket service first to stop the kernel from polling */
    /* This is safe to call even if registration failed or hasn't happened yet */
    (void)net_socket_service_unregister(&udp_motor_service);

    /* 2. Close the left socket */
    if (socket_left >= 0) {
        LOG_DBG("Closing left socket (%d)", socket_left);
        close(socket_left);
        socket_left = -1;
    }

    /* 3. Close the right socket */
    if (socket_right >= 0) {
        LOG_DBG("Closing right socket (%d)", socket_right);
        close(socket_right);
        socket_right = -1;
    }

    /* 4. Reset the pollfd structures to be safe */
    memset(sockfd_udp, 0, sizeof(sockfd_udp));
    sockfd_udp[0].fd = -1;
    sockfd_udp[1].fd = -1;
}

static int handle_motor_service_error(const char *msg, int err_code)
{
    LOG_ERR("%s: %d", msg, err_code);
    close_motor_sockets(); // Centralized cleanup
    return err_code;
}

static int start_motor_service(void)
{
    struct sockaddr_in addr_left = {
            .sin_family = AF_INET,
            .sin_port = htons(PORT_LEFT),
            .sin_addr.s_addr = INADDR_ANY,
    };
    struct sockaddr_in addr_right = {
            .sin_family = AF_INET,
            .sin_port = htons(PORT_RIGHT),
            .sin_addr.s_addr = INADDR_ANY,
    };
    int ret;

    socket_left = setup_socket_and_bind(&addr_left);
    if (socket_left < 0) {
        return handle_motor_service_error("Failed to setup left socket", socket_left);
    }

    socket_right = setup_socket_and_bind(&addr_right);
    if (socket_right < 0) {
        return handle_motor_service_error("Failed to setup right socket", socket_right);
    }

    sockfd_udp[0].fd = socket_left;
    sockfd_udp[0].events = POLLIN;
    sockfd_udp[1].fd = socket_right;
    sockfd_udp[1].events = POLLIN;

    ret = net_socket_service_register(&udp_motor_service, sockfd_udp, ARRAY_SIZE(sockfd_udp), NULL);
    if (ret < 0) {
        return handle_motor_service_error("Failed to register socket service", ret);
    }

    LOG_INF("Motor service started successfully.");
    return 0;
}

SYS_INIT(start_motor_service, APPLICATION, 99);

int main(void)
{
    while (1) {
        k_sleep(K_FOREVER);
    }
}
