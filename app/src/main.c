#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>

#include <zephyr/net/socket.h>

int main(void) {
    int sock = zsock_socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    struct sockaddr_in addr = {
        .sin_family = AF_INET,
        .sin_port = htons(3000),
        .sin_addr.s_addr = INADDR_ANY,
    };

    zsock_bind(sock, (struct sockaddr *)&addr, sizeof(addr));

    char buf[64];
    while (1) {
        int len = zsock_recv(sock, buf, sizeof(buf) - 1, 0);
        if (len > 0) {
            buf[len] = '\0';
            printk("Received: %s\n", buf);
        }
    }
}
