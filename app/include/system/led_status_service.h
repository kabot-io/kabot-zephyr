#ifndef KABOT_SYSTEM_LED_STATUS_SERVICE_H_
#define KABOT_SYSTEM_LED_STATUS_SERVICE_H_

#include <stdbool.h>

enum led_status_global_mode {
	LED_STATUS_GLOBAL_IDLE,
	LED_STATUS_GLOBAL_UPDATE,
	LED_STATUS_GLOBAL_EXECUTION,
	LED_STATUS_GLOBAL_WARNING,
	LED_STATUS_GLOBAL_ERROR,
};

int led_status_service_init(void);
void led_status_service_set_global_mode(enum led_status_global_mode mode);
void led_status_service_set_network_ready(bool ready);
void led_status_service_set_claimed(bool claimed);
void led_status_service_notify_tx(void);
void led_status_service_notify_rx_udp(void);
void led_status_service_notify_rx_decode(void);

#endif /* KABOT_SYSTEM_LED_STATUS_SERVICE_H_ */
