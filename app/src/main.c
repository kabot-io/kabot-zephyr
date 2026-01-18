#include <zephyr/kernel.h>
#include <zephyr/init.h>
#include "motor_service.h"

// Automatic startup using Zephyr's init system
static int kabot_init(void)
{
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
