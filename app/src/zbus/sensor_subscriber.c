#include "zbus/sensor_subscriber.h"
#include "zbus/sensor_channel.h"

#include <errno.h>
#include <math.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(sensor_subscriber, LOG_LEVEL_DBG);

ZBUS_SUBSCRIBER_DEFINE(sensor_subscriber, 1);

K_THREAD_STACK_DEFINE(sensor_subscriber_stack, CONFIG_MAIN_STACK_SIZE);
static struct k_thread sensor_subscriber_thread;
static k_tid_t sensor_subscriber_tid;

void sensor_subscriber_task(void)
{
    const struct zbus_channel *chan;

    while (!zbus_sub_wait(&sensor_subscriber, &chan, K_FOREVER)) {
        if (&sensor_channel != chan) {
            continue;
        }

        struct sensor_msg sensor;
        if (zbus_chan_read(&sensor_channel, &sensor, K_MSEC(20)) == 0) {
            const float left_q31 = (float)sensor.left_encoder.readings[0].value / 2147483648.0f;
            const float right_q31 = (float)sensor.right_encoder.readings[0].value / 2147483648.0f;
            const float left_value = ldexpf(left_q31, sensor.left_encoder.shift);
            const float right_value = ldexpf(right_q31, sensor.right_encoder.shift);

                    LOG_INF("Sensor tuple: left=(ts=%llu,value=%f) right=(ts=%llu,value=%f)",
                        (unsigned long long)sensor.left_encoder.header.base_timestamp_ns,
                        left_value,
                        (unsigned long long)sensor.right_encoder.header.base_timestamp_ns,
                        right_value);
        }
    }
}

int start_sensor_subscriber(void)
{
    if (sensor_subscriber_tid != NULL) {
        return -EALREADY;
    }

    sensor_subscriber_tid = k_thread_create(&sensor_subscriber_thread, sensor_subscriber_stack,
                                            K_THREAD_STACK_SIZEOF(sensor_subscriber_stack),
                                            (k_thread_entry_t)sensor_subscriber_task, NULL, NULL,
                                            NULL, 3, 0, K_NO_WAIT);

    if (sensor_subscriber_tid == NULL) {
        return -EIO;
    }

    return 0;
}

void stop_sensor_subscriber(void)
{
    if (sensor_subscriber_tid == NULL) {
        return;
    }

    k_thread_abort(sensor_subscriber_tid);
    sensor_subscriber_tid = NULL;
}
