/* SPDX-License-Identifier: Apache-2.0 */
/* SPDX-FileCopyrightText: Copyright The Kabot Project Contributors */

#include "system/led_status_service.h"

#include <errno.h>

#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/led_strip.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/random/random.h>

LOG_MODULE_REGISTER(led_status_service, LOG_LEVEL_DBG);

#define STRIP_NODE DT_NODELABEL(led_strip)

BUILD_ASSERT(DT_NODE_EXISTS(STRIP_NODE), "devicetree node 'led_strip' not found");

#define STRIP_NUM_PIXELS DT_PROP(STRIP_NODE, chain_length)

#define BASE_R 2
#define BASE_G 0
#define BASE_B 1

#define BREATHE_R 10
#define BREATHE_G 0
#define BREATHE_B 5

#define SPARKLE_R 10
#define SPARKLE_G 10
#define SPARKLE_B 10

#define BREATHE_STEPS 20

#define UPDATE_R 0
#define UPDATE_G 0
#define UPDATE_B 1

#define EXECUTION_R 1
#define EXECUTION_G 1
#define EXECUTION_B 1

#define WARNING_R 1
#define WARNING_G 1
#define WARNING_B 0

#define ERROR_R 1
#define ERROR_G 0
#define ERROR_B 0

#define NET_BAD_R 1
#define NET_BAD_G 0
#define NET_BAD_B 0

#define NET_GOOD_R 1
#define NET_GOOD_G 1
#define NET_GOOD_B 0

#define TX_R 0
#define TX_G 0
#define TX_B 0

#define RX_UDP_R 0
#define RX_UDP_G 0
#define RX_UDP_B 1

#define RX_DECODE_R 0
#define RX_DECODE_G 0
#define RX_DECODE_B 0

enum {
	LED_TX = 3,
	LED_RX = 4,
};

static const struct device *const strip = DEVICE_DT_GET(STRIP_NODE);
static struct led_rgb pixels[STRIP_NUM_PIXELS];

static struct k_mutex led_lock;
static struct k_work_delayable tx_off_work;
static struct k_work_delayable rx_off_work;
static struct k_work_delayable execution_anim_work;
static struct k_work_delayable status_anim_work;

static bool service_ready;
static bool network_ready;
static bool tx_blink_active;
static bool rx_blink_active;
static struct led_rgb rx_blink_color;
static enum led_status_global_mode global_mode;
static bool execution_phase_bright;
static bool sparkle_active;
static size_t sparkle_led;
static int8_t breathe_phase;
static int8_t breathe_direction;

static void execution_anim_work_handler(struct k_work *work);
static void status_anim_work_handler(struct k_work *work);

static const struct led_rgb GLOBAL_COLORS[] = {
	[LED_STATUS_GLOBAL_IDLE] = {.r = BASE_R, .g = BASE_G, .b = BASE_B},
	[LED_STATUS_GLOBAL_UPDATE] = {.r = UPDATE_R, .g = UPDATE_G, .b = UPDATE_B},
	[LED_STATUS_GLOBAL_EXECUTION] = {.r = EXECUTION_R, .g = EXECUTION_G, .b = EXECUTION_B},
	[LED_STATUS_GLOBAL_WARNING] = {.r = WARNING_R, .g = WARNING_G, .b = WARNING_B},
	[LED_STATUS_GLOBAL_ERROR] = {.r = ERROR_R, .g = ERROR_G, .b = ERROR_B},
};

static inline bool led_index_valid(size_t idx)
{
	return idx < STRIP_NUM_PIXELS;
}

static inline void set_rgb(size_t idx, uint8_t r, uint8_t g, uint8_t b)
{
	if (!led_index_valid(idx)) {
		return;
	}

	pixels[idx].r = r;
	pixels[idx].g = g;
	pixels[idx].b = b;
}

static struct led_rgb global_background_color_locked(void)
{
	if (!network_ready) {
		return (struct led_rgb){
			.r = (uint8_t)((BREATHE_R * breathe_phase) / BREATHE_STEPS),
			.g = (uint8_t)((BREATHE_G * breathe_phase) / BREATHE_STEPS),
			.b = (uint8_t)((BREATHE_B * breathe_phase) / BREATHE_STEPS),
		};
	}

	if (global_mode == LED_STATUS_GLOBAL_EXECUTION) {
		if (execution_phase_bright) {
			return GLOBAL_COLORS[LED_STATUS_GLOBAL_EXECUTION];
		}

		return GLOBAL_COLORS[LED_STATUS_GLOBAL_IDLE];
	}

	if (global_mode < LED_STATUS_GLOBAL_IDLE || global_mode > LED_STATUS_GLOBAL_ERROR) {
		return GLOBAL_COLORS[LED_STATUS_GLOBAL_IDLE];
	}

	return GLOBAL_COLORS[global_mode];
}

static void schedule_execution_anim_locked(void)
{
	if (global_mode == LED_STATUS_GLOBAL_EXECUTION) {
		(void)k_work_reschedule(&execution_anim_work, K_SECONDS(1));
	} else {
		(void)k_work_cancel_delayable(&execution_anim_work);
	}
}

static void schedule_status_anim_locked(k_timeout_t delay)
{
	(void)k_work_reschedule(&status_anim_work, delay);
}

static int32_t breathe_step_ms(void)
{
	return MAX(1, CONFIG_KABOT_LED_STATUS_BREATHE_MS / BREATHE_STEPS);
}

static int32_t sparkle_rest_ms(void)
{
	return MAX(1, CONFIG_KABOT_LED_STATUS_SPARKLE_PERIOD_MS -
			      CONFIG_KABOT_LED_STATUS_SPARKLE_PULSE_MS);
}

static void apply_pixels_locked(void)
{
	struct led_rgb bg = global_background_color_locked();

	for (size_t i = 0; i < STRIP_NUM_PIXELS; i++) {
		set_rgb(i, bg.r, bg.g, bg.b);
	}

	if (network_ready && sparkle_active) {
		set_rgb(sparkle_led, SPARKLE_R, SPARKLE_G, SPARKLE_B);
	}

	if (tx_blink_active) {
		set_rgb(LED_TX, TX_R, TX_G, TX_B);
	}

	if (rx_blink_active) {
		set_rgb(LED_RX, rx_blink_color.r, rx_blink_color.g, rx_blink_color.b);
	}

	int rc = led_strip_update_rgb(strip, pixels, STRIP_NUM_PIXELS);
	if (rc < 0) {
		LOG_WRN("led_strip_update_rgb failed: %d", rc);
	}
}

static void status_anim_work_handler(struct k_work *work)
{
	ARG_UNUSED(work);

	if (!service_ready) {
		return;
	}

	k_mutex_lock(&led_lock, K_FOREVER);
	if (!network_ready) {
		sparkle_active = false;
		breathe_phase += breathe_direction;
		if (breathe_phase >= BREATHE_STEPS) {
			breathe_phase = BREATHE_STEPS;
			breathe_direction = -1;
		} else if (breathe_phase <= 0) {
			breathe_phase = 0;
			breathe_direction = 1;
		}

		apply_pixels_locked();
		schedule_status_anim_locked(K_MSEC(breathe_step_ms()));
	} else if (sparkle_active) {
		sparkle_active = false;
		apply_pixels_locked();
		schedule_status_anim_locked(K_MSEC(sparkle_rest_ms()));
	} else {
		sparkle_led = (size_t)(sys_rand32_get() % STRIP_NUM_PIXELS);
		sparkle_active = true;
		apply_pixels_locked();
		schedule_status_anim_locked(K_MSEC(CONFIG_KABOT_LED_STATUS_SPARKLE_PULSE_MS));
	}
	k_mutex_unlock(&led_lock);
}

static void execution_anim_work_handler(struct k_work *work)
{
	ARG_UNUSED(work);

	if (!service_ready) {
		return;
	}

	k_mutex_lock(&led_lock, K_FOREVER);
	if (global_mode != LED_STATUS_GLOBAL_EXECUTION) {
		k_mutex_unlock(&led_lock);
		return;
	}

	execution_phase_bright = !execution_phase_bright;
	apply_pixels_locked();
	schedule_execution_anim_locked();
	k_mutex_unlock(&led_lock);
}

static void tx_off_work_handler(struct k_work *work)
{
	ARG_UNUSED(work);

	if (!service_ready) {
		return;
	}

	k_mutex_lock(&led_lock, K_FOREVER);
	tx_blink_active = false;
	apply_pixels_locked();
	k_mutex_unlock(&led_lock);
}

static void rx_off_work_handler(struct k_work *work)
{
	ARG_UNUSED(work);

	if (!service_ready) {
		return;
	}

	k_mutex_lock(&led_lock, K_FOREVER);
	rx_blink_active = false;
	apply_pixels_locked();
	k_mutex_unlock(&led_lock);
}

int led_status_service_init(void)
{
	if (service_ready) {
		return 0;
	}

	if (!device_is_ready(strip)) {
		return -ENODEV;
	}

	k_mutex_init(&led_lock);
	k_work_init_delayable(&tx_off_work, tx_off_work_handler);
	k_work_init_delayable(&rx_off_work, rx_off_work_handler);
	k_work_init_delayable(&execution_anim_work, execution_anim_work_handler);
	k_work_init_delayable(&status_anim_work, status_anim_work_handler);

	k_mutex_lock(&led_lock, K_FOREVER);
	service_ready = true;
	network_ready = false;
	tx_blink_active = false;
	rx_blink_active = false;
	sparkle_active = false;
	sparkle_led = 0;
	breathe_phase = 0;
	breathe_direction = 1;
	global_mode = LED_STATUS_GLOBAL_IDLE;
	execution_phase_bright = false;
	rx_blink_color = (struct led_rgb){
		.r = RX_UDP_R,
		.g = RX_UDP_G,
		.b = RX_UDP_B,
	};
	schedule_execution_anim_locked();
	apply_pixels_locked();
	schedule_status_anim_locked(K_NO_WAIT);
	k_mutex_unlock(&led_lock);

	LOG_INF("LED status service ready");
	return 0;
}

void led_status_service_set_global_mode(enum led_status_global_mode mode)
{
	if (!service_ready) {
		return;
	}

	if (mode < LED_STATUS_GLOBAL_IDLE || mode > LED_STATUS_GLOBAL_ERROR) {
		mode = LED_STATUS_GLOBAL_IDLE;
	}

	k_mutex_lock(&led_lock, K_FOREVER);
	if (global_mode != mode) {
		global_mode = mode;
		if (global_mode == LED_STATUS_GLOBAL_EXECUTION) {
			execution_phase_bright = true;
		}
		schedule_execution_anim_locked();
		apply_pixels_locked();
	}
	k_mutex_unlock(&led_lock);
}

void led_status_service_set_network_ready(bool ready)
{
	if (!service_ready) {
		return;
	}

	k_mutex_lock(&led_lock, K_FOREVER);
	if (network_ready != ready) {
		network_ready = ready;
		sparkle_active = false;
		breathe_phase = 0;
		breathe_direction = 1;
		apply_pixels_locked();
		schedule_status_anim_locked(K_NO_WAIT);
	}
	k_mutex_unlock(&led_lock);
}

void led_status_service_notify_tx(void)
{
	if (!service_ready) {
		return;
	}

	k_mutex_lock(&led_lock, K_FOREVER);
	tx_blink_active = true;
	apply_pixels_locked();
	(void)k_work_reschedule(&tx_off_work, K_MSEC(CONFIG_KABOT_LED_STATUS_BLINK_MS));
	k_mutex_unlock(&led_lock);
}

void led_status_service_notify_rx_udp(void)
{
	if (!service_ready) {
		return;
	}

	k_mutex_lock(&led_lock, K_FOREVER);
	rx_blink_active = true;
	rx_blink_color = (struct led_rgb){
		.r = RX_UDP_R,
		.g = RX_UDP_G,
		.b = RX_UDP_B,
	};
	apply_pixels_locked();
	(void)k_work_reschedule(&rx_off_work, K_MSEC(CONFIG_KABOT_LED_STATUS_BLINK_MS));
	k_mutex_unlock(&led_lock);
}

void led_status_service_notify_rx_decode(void)
{
	if (!service_ready) {
		return;
	}

	k_mutex_lock(&led_lock, K_FOREVER);
	rx_blink_active = true;
	rx_blink_color = (struct led_rgb){
		.r = RX_DECODE_R,
		.g = RX_DECODE_G,
		.b = RX_DECODE_B,
	};
	apply_pixels_locked();
	(void)k_work_reschedule(&rx_off_work, K_MSEC(CONFIG_KABOT_LED_STATUS_BLINK_MS));
	k_mutex_unlock(&led_lock);
}
