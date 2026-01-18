#include <zephyr/kernel.h>
#include <zephyr/init.h>
#include "motor_service.h"

#ifdef CONFIG_ZTEST
#include <zephyr/ztest.h>

#if !defined(CONFIG_APP_RUN_TESTS)
/* SCENARIO 1: Define the missing global for the linker */
#include <zephyr/fff.h> // Required for FFF macros
DEFINE_FFF_GLOBALS;
#endif

#endif

/* Standard Init Logic */
static int kabot_init(void) {
    return start_motor_service();
}
SYS_INIT(kabot_init, APPLICATION, 99);

/* --- ENTRY POINT LOGIC --- */

#ifdef CONFIG_ZTEST
void test_main(void) {
#if defined(CONFIG_APP_RUN_TESTS)
    /* SCENARIO 2: UTs */
    ztest_run_all(NULL, false, 1, 1);
#else
    /* SCENARIO 1: PC Full App */
    printk("Kabot App starting on PC (Simulation)...\n");
    while (1) {
        k_sleep(K_FOREVER);
    }
#endif
}
#else
/* SCENARIO 3: ESP32 */
int main(void) {
    while (1) {
        k_sleep(K_FOREVER);
    }
}
#endif
