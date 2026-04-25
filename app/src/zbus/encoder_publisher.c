#include "zbus/encoder_publisher.h"

#include "zbus/sensor_channel.h"
#include "zbus/sensor_msg.h"

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(encoder_publisher, LOG_LEVEL_DBG);

void encoder_publisher_task(void)
{
    q31_t value = 0;

    while (1) {
        struct sensor_msg msg = {0};

        msg.data.header.base_timestamp_ns = (uint64_t)k_uptime_get() * 1000000ULL;
        msg.data.shift = 0;
        msg.data.readings[0].timestamp_delta = 0;
        msg.data.readings[0].value = value;

        int rc = publish_sensor_msg(&msg, K_MSEC(20));
        if (rc == 0) {
            LOG_INF("Published dummy sensor value=%d", msg.data.readings[0].value);
        } else {
            LOG_WRN("Failed to publish dummy sensor value: %d", rc);
        }

        value += 1000;
        if (value > 10000) {
            value = 0;
        }

        k_sleep(K_SECONDS(1));
    }
}

K_THREAD_DEFINE(encoder_publisher_task_id, CONFIG_MAIN_STACK_SIZE, encoder_publisher_task, NULL,
                NULL, NULL, 4, 0, 0);
