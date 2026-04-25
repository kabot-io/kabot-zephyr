#pragma once

#include "zbus/sensor_msg.h"
#include <zephyr/zbus/zbus.h>

/**
 * @brief Subscriber task for processing sensor messages from the sensor_channel.
 *
 * This function runs in its own thread and waits for messages on the sensor_channel.
 */
void sensor_subscriber_task(void);
