#include "zbus/sensor_subscriber.h"
#include "zbus/sensor_channel.h"

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(sensor_subscriber, LOG_LEVEL_DBG);

ZBUS_SUBSCRIBER_DEFINE(sensor_subscriber, 1);

void sensor_subscriber_task(void)
{
    const struct zbus_channel *chan;

    while (!zbus_sub_wait(&sensor_subscriber, &chan, K_FOREVER)) {
        if (&sensor_channel != chan) {
            continue;
        }

        struct sensor_msg sensor;
        if (zbus_chan_read(&sensor_channel, &sensor, K_MSEC(20)) == 0) {
            LOG_INF("Sensor tuple: left=(ts=%llu,value=%d,shift=%d) "
                    "right=(ts=%llu,value=%d,shift=%d)",
                    (unsigned long long)sensor.left_encoder.header.base_timestamp_ns,
                    sensor.left_encoder.readings[0].value, sensor.left_encoder.shift,
                    (unsigned long long)sensor.right_encoder.header.base_timestamp_ns,
                    sensor.right_encoder.readings[0].value, sensor.right_encoder.shift);
        }
    }
}

K_THREAD_DEFINE(sensor_subscriber_task_id, CONFIG_MAIN_STACK_SIZE, sensor_subscriber_task, NULL,
                NULL, NULL, 3, 0, 0);
