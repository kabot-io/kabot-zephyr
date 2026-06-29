#include "control/discovery_service.h"

#include "protos/state_control_msg.pb.h"
#include "system/robot_settings.h"
#if defined(CONFIG_LED_STRIP)
#include "system/led_status_service.h"
#endif

#include <arpa/inet.h>
#include <errno.h>
#include <pb_decode.h>
#include <pb_encode.h>
#include <string.h>
#include <zephyr/logging/log.h>
#include <zephyr/net/socket_service.h>
#include <zephyr/posix/poll.h>
#include <zephyr/posix/sys/socket.h>
#include <zephyr/posix/unistd.h>
#include <zephyr/sys/printk.h>
#include <zephyr/app_version.h>

LOG_MODULE_REGISTER(discovery_service, LOG_LEVEL_DBG);

enum {
	DISCOVERY_MTU = 256,
	RESPONSE_MTU = 384,
};

static int discovery_socket = -1;
static struct pollfd discovery_pollfd;

static bool encode_string_cb(pb_ostream_t *stream, const pb_field_iter_t *field, void *const *arg)
{
	const char *text = (const char *)(*arg);
	if (text == NULL) {
		text = "";
	}

	if (!pb_encode_tag_for_field(stream, field)) {
		return false;
	}

	return pb_encode_string(stream, (const pb_byte_t *)text, strlen(text));
}

static int setup_socket(uint16_t port)
{
	int sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if (sock < 0) {
		return -errno;
	}

	struct sockaddr_in addr = {
		.sin_family = AF_INET,
		.sin_port = htons(port),
		.sin_addr.s_addr = INADDR_ANY,
	};

	if (bind(sock, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
		int err = errno;
		close(sock);
		return -err;
	}

	return sock;
}

static void handle_bonjour_packet(int fd, struct sockaddr_in *sender_addr, const uint8_t *buf,
				  size_t len)
{
	Bonjour msg = Bonjour_init_zero;
	pb_istream_t stream = pb_istream_from_buffer((const pb_byte_t *)buf, len);
	if (!pb_decode(&stream, Bonjour_fields, &msg)) {
		LOG_WRN("Ignoring malformed Bonjour datagram (%u bytes)", (unsigned int)len);
		return;
	}

	if (!msg.release && ((msg.hmi_port == 0U) || (msg.hmi_port > 65535U))) {
		LOG_WRN("Ignoring Bonjour with invalid hmi_port=%u", (unsigned int)msg.hmi_port);
		return;
	}

	char sender_ip[KABOT_IPV4_STR_LEN] = {0};
	if (inet_ntop(AF_INET, &sender_addr->sin_addr, sender_ip, sizeof(sender_ip)) == NULL) {
		(void)snprintk(sender_ip, sizeof(sender_ip), "0.0.0.0");
	}

	uint16_t sender_port = ntohs(sender_addr->sin_port);
	uint16_t hmi_port = (uint16_t)msg.hmi_port;

	LOG_INF("Bonjour RX from %s:%u hmi_port=%u claim=%s release=%s", sender_ip,
		(unsigned int)sender_port, (unsigned int)hmi_port, msg.claim ? "true" : "false",
		msg.release ? "true" : "false");

	if (msg.release) {
		bool was_claimed = robot_settings_is_claimed();
		robot_settings_clear_claim();
#if defined(CONFIG_LED_STRIP)
		led_status_service_set_claimed(false);
#endif
		LOG_INF("Bonjour RELEASE: claimed %s -> false (stream disabled)",
			was_claimed ? "true" : "false");
	} else if (msg.claim) {
		char old_ip[KABOT_IPV4_STR_LEN] = {0};
		uint16_t old_port = 0;
		robot_settings_get_hmi_target(old_ip, sizeof(old_ip), &old_port);

		int persist_rc = robot_settings_set_hmi_target(sender_ip, hmi_port);

#if defined(CONFIG_LED_STRIP)
		led_status_service_set_claimed(true);
#endif

		char new_ip[KABOT_IPV4_STR_LEN] = {0};
		uint16_t new_port = 0;
		robot_settings_get_hmi_target(new_ip, sizeof(new_ip), &new_port);

		LOG_INF("Bonjour CLAIM takeover: %s:%u -> %s:%u", old_ip, (unsigned int)old_port,
			new_ip, (unsigned int)new_port);

		if (persist_rc < 0) {
			LOG_WRN("Bonjour CLAIM settings save failed: %d", persist_rc);
		} else {
			LOG_INF("Bonjour CLAIM settings save rc=%d", persist_rc);
		}
	} else {
		LOG_INF("Bonjour DISCOVER only from %s:%u (no takeover)", sender_ip,
			(unsigned int)sender_port);
	}

	char serial[KABOT_SERIAL_MAX_LEN] = {0};
	char human_name[KABOT_HUMAN_NAME_MAX_LEN] = {0};
	char claimed_by_ip[KABOT_IPV4_STR_LEN] = {0};
	bool is_claimed = false;
	robot_settings_get_identity(serial, sizeof(serial), human_name, sizeof(human_name));
	robot_settings_get_claim_state(&is_claimed, claimed_by_ip, sizeof(claimed_by_ip));

	BonjourResponse response = BonjourResponse_init_zero;
	response.serial.funcs.encode = encode_string_cb;
	response.serial.arg = serial;
	response.human_name.funcs.encode = encode_string_cb;
	response.human_name.arg = human_name;
	response.control_port = (uint32_t)CONFIG_KABOT_CONTROL_INGRESS_PORT;
	response.firmware_version.funcs.encode = encode_string_cb;
	response.firmware_version.arg = (void *)APP_VERSION_TWEAK_STRING;
	response.is_claimed = is_claimed;
	response.claimed_by_ip.funcs.encode = encode_string_cb;
	response.claimed_by_ip.arg = claimed_by_ip;

	uint8_t encode_buf[RESPONSE_MTU];
	pb_ostream_t out = pb_ostream_from_buffer(encode_buf, sizeof(encode_buf));
	if (!pb_encode(&out, BonjourResponse_fields, &response)) {
		LOG_WRN("Failed to encode BonjourResponse: %s", PB_GET_ERROR(&out));
		return;
	}

	ssize_t sent = sendto(fd, encode_buf, out.bytes_written, 0, (struct sockaddr *)sender_addr,
			      sizeof(*sender_addr));
	if (sent < 0) {
		LOG_WRN("Bonjour response send failed: %d", errno);
		return;
	}

	const char *mode = msg.release ? "release" : (msg.claim ? "claim" : "discover");
	LOG_INF("Bonjour response TX mode=%s to %s:%u serial=%s name=%s claimed=%s claimed_by=%s "
		"control_port=%u fw=%s bytes=%d",
		mode, sender_ip, (unsigned int)sender_port, serial, human_name,
		response.is_claimed ? "true" : "false", claimed_by_ip[0] ? claimed_by_ip : "",
		(unsigned int)response.control_port, APP_VERSION_TWEAK_STRING, (int)sent);
}

static void udp_discovery_handler(struct net_socket_service_event *pev)
{
	struct pollfd *pfd = &pev->event;
	struct sockaddr_in sender_addr;
	socklen_t addrlen = sizeof(sender_addr);
	static uint8_t buf[DISCOVERY_MTU];

	int len = recvfrom(pfd->fd, buf, sizeof(buf), 0, (struct sockaddr *)&sender_addr, &addrlen);
	if (len < 0) {
		LOG_ERR("Discovery recvfrom failed with error %d", errno);
		return;
	}

	if (len == 0) {
		return;
	}

	if (pfd->fd != discovery_socket) {
		LOG_ERR("Discovery data received on unknown socket: %d", pfd->fd);
		return;
	}

	handle_bonjour_packet(discovery_socket, &sender_addr, buf, (size_t)len);
}

NET_SOCKET_SERVICE_SYNC_DEFINE(udp_discovery_service, udp_discovery_handler, 1);

void stop_discovery_service(void)
{
	(void)net_socket_service_unregister(&udp_discovery_service);
	if (discovery_socket >= 0) {
		close(discovery_socket);
		discovery_socket = -1;
	}
}

int start_discovery_service(void)
{
	discovery_socket = setup_socket((uint16_t)CONFIG_KABOT_DISCOVERY_PORT);
	if (discovery_socket < 0) {
		return discovery_socket;
	}

	discovery_pollfd.fd = discovery_socket;
	discovery_pollfd.events = POLLIN;

	int rc = net_socket_service_register(&udp_discovery_service, &discovery_pollfd, 1, NULL);
	if (rc < 0) {
		stop_discovery_service();
		return rc;
	}

	LOG_INF("Discovery service active on port %d (protobuf Bonjour)",
		CONFIG_KABOT_DISCOVERY_PORT);
	return 0;
}
