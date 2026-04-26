#pragma once

#include "zbus/sensor_msg.h"
#include <zephyr/zbus/zbus.h>

/**
 * @brief Subscriber task for processing sensor messages from the sensor_channel.
 *
 * This function runs in its own thread and waits for messages on the sensor_channel.
 */
void sensor_subscriber_task(void);

/**
 * @brief Start the sensor subscriber thread.
 *
 * @return 0 on success, negative errno on failure.
 */
int start_sensor_subscriber(void);

/**
 * @brief Stop the sensor subscriber thread.
 */
void stop_sensor_subscriber(void);
