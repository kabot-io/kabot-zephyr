#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>

int main(void)
{
    int counter = 0;
    while (1) {
        printk("%d\n", counter++);
        k_sleep(K_MSEC(1000));
    }
    return 0;
}
