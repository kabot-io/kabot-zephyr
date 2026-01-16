#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>

BUILD_ASSERT(DT_NODE_HAS_COMPAT(DT_CHOSEN(zephyr_console), zephyr_native_pty_uart) || DT_NODE_HAS_COMPAT(DT_CHOSEN(zephyr_console), espressif_esp32_usb_serial), "Unsupported console device");

int main(void)
{
    const struct device *dev = DEVICE_DT_GET(DT_CHOSEN(zephyr_console));

    /* Check if the device is ready */
    if (!device_is_ready(dev)) {
        printk("Console device not ready\n");
        return 0;
    }

    printk("--- USB Serial/JTAG Connected! ---\n");

    int counter = 0;
    while (1) {
        printk("Counter value: %d\n", counter++);
        k_sleep(K_MSEC(1000));
    }
    return 0;
}
