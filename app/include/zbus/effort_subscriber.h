#pragma once

#include "zbus/effort_msg.h"
#include <zephyr/zbus/zbus.h>

/**
 * @brief Subscriber task for processing effort messages from the effort_channel.
 *
 * This function runs in its own thread and waits for messages on the effort_channel.
 */
void effort_subscriber_task(void);

/**
 * @brief Initialize all motor driver backends.
 *
 * Selects the correct backend (ESC, H-Bridge, simulation stub, …) at build
 * time and calls their init routines.
 *
 * @return 0 on success, negative errno on failure.
 */
int initialize_motor_drivers(void);
