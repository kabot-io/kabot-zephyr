#pragma once

/**
 * @brief Initialize and start the UDP motor control service.
 *
 * This function sets up the UDP sockets and starts listening for motor control commands.
 *
 * @return 0 on success, negative errno on failure.
 */
int start_motor_service(void);

/**
 * @brief Stop the motor control service and release all socket resources.
 *
 * This function closes the UDP socket and cleans up any resources used by the service.
 */
void stop_motor_service(void);
