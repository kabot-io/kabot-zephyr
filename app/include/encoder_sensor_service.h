#pragma once

/**
 * @brief Initialize and start the UDP encoder sensor service.
 *
 * This function sets up the UDP sockets and starts listening for encoder sensor data.
 *
 * @return 0 on success, negative errno on failure.
 */
int start_encoder_sensor_service(void);

/**
 * @brief Stop the encoder sensor service and release all socket resources.
 *
 * This function closes the UDP socket and cleans up any resources used by the service.
 */
void stop_encoder_sensor_service(void);
