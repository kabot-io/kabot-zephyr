#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define KABOT_IPV4_STR_LEN       16
#define KABOT_SERIAL_MAX_LEN     64
#define KABOT_HUMAN_NAME_MAX_LEN 64

int robot_settings_init(void);

void robot_settings_get_identity(char *serial_out, size_t serial_out_len, char *human_name_out,
				 size_t human_name_out_len);

void robot_settings_get_hmi_target(char *ip_out, size_t ip_out_len, uint16_t *port_out);

int robot_settings_set_hmi_target(const char *ip, uint16_t port);

bool robot_settings_is_claimed(void);

void robot_settings_clear_claim(void);

void robot_settings_get_claim_state(bool *claimed_out, char *claimed_by_ip_out,
				    size_t claimed_by_ip_out_len);
