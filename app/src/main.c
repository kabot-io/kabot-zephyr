#include "zephyr/net/net_ip.h"
#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(kabot, LOG_LEVEL_DBG);

#include <zephyr/posix/sys/socket.h>
#include <zephyr/posix/unistd.h>
#include <zephyr/net/socket_service.h>
#include <zephyr/posix/poll.h>

#define PORT_LEFT 30100
#define PORT_RIGHT 30200

static void udp_motor_service_handler(struct net_socket_service_event *pev)
{
	return;
}

NET_SOCKET_SERVICE_SYNC_DEFINE(udp_motor_service, udp_motor_service_handler, 2);

int setup_socket_and_bind(struct sockaddr* addr) {
    int sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if(bind(sock, addr, sizeof(*addr)) < 0) {
        LOG_ERR("bind: %d", -errno);
        return -errno;
    }
    return sock;
}

int main(void) {

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

    int socket_left = setup_socket_and_bind((struct sockaddr*)&addr_left);
    int socket_right = setup_socket_and_bind((struct sockaddr*)&addr_right);

    char buf_left[64];
    char buf_right[64];
    while (1) {
        int len_left = zsock_recv(socket_left, buf_left, sizeof(buf_left) - 1, 0);
        if (len_left > 0) {
            buf_left[len_left] = '\0';
            printk("Received left: %s\n", buf_left);
        }
        int len_right = zsock_recv(socket_right, buf_right, sizeof(buf_right) - 1, 0);
        if (len_right > 0) {
            buf_right[len_right] = '\0';
            printk("Received right: %s\n", buf_right);
        }
    }
}
