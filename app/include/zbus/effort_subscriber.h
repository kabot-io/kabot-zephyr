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
int set_motor_effort(struct effort_msg *effort);

/**
 * @brief Maps effort (-100 to 100) to pulse width (1500us to 2500us)
 * * @param effort Value between -100 and 100
 * @param flip   Boolean to reverse the direction
 * @return uint32_t Pulse width in microseconds
 */
uint32_t map_effort_to_pulse(int32_t effort, bool flip);
