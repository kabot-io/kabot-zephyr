#pragma once

#include "zbus/effort_msg.h"
#include <zephyr/zbus/zbus.h>

/**
 * @brief Subscriber task for processing effort messages from the effort_channel.
 *
 * This function runs in its own thread and waits for messages on the effort_channel.
 */
void effort_subscriber_task(void);
int initialize_motor_pwms(void);
