#pragma once

/**
 * @brief Initialize and start the UDP control ingress service.
 *
 * This function sets up UDP sockets and starts listening for Control protobuf datagrams.
 *
 * @return 0 on success, negative errno on failure.
 */
int start_control_service(void);

/**
 * @brief Stop the control ingress service and release all socket resources.
 */
void stop_control_service(void);
