#pragma once

#include "zbus/effort_msg.h"

#include <zephyr/zbus/zbus.h>

void effort_subscriber_task(void);
int initialize_motor_drivers(void);
