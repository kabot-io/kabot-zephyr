#include "zbus/sensor_channel.h"
#include "zbus/sensor_msg.h"
#include "zbus/sensor_subscriber.h"

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(sensor_channel, LOG_LEVEL_DBG);

int publish_sensor_msg(const struct sensor_msg *msg, k_timeout_t timeout)
{
    return zbus_chan_pub(&sensor_channel, msg, timeout);
}

bool sensor_channel_validator(const void *msg, size_t msg_size)
{
    return (msg != NULL) && (msg_size == sizeof(struct sensor_msg));
}

ZBUS_CHAN_DEFINE(sensor_channel, struct sensor_msg, sensor_channel_validator, NULL,
                 ZBUS_OBSERVERS(sensor_subscriber), ZBUS_MSG_INIT(0));
