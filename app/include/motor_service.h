#ifndef KABOT_MOTOR_SERVICE_H
#define KABOT_MOTOR_SERVICE_H

/**
 * @brief Initialize and start the UDP motor control service.
 * * @return 0 on success, negative errno on failure.
 */
int start_motor_service(void);

/**
 * @brief Stop the service and release all socket resources.
 */
void stop_motor_service(void);

#endif /* KABOT_MOTOR_SERVICE_H */
