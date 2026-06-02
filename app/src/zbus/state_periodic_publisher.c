#include "zbus/state_channel.h"
#include "zbus/state_egress_channel.h"

#include <errno.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(state_periodic_publisher, LOG_LEVEL_DBG);

ZBUS_SUBSCRIBER_DEFINE(state_periodic_publisher, 4);

enum {
    READ_TIMEOUT_MS = 20,
    PUBLISH_TIMEOUT_MS = 20,
};

static bool is_newer_stamp(uint64_t incoming, uint64_t current)
{
    return incoming >= current;
}

static bool should_replace_field(bool incoming_has_header,
                                 uint64_t incoming_stamp,
                                 bool has_current,
                                 bool current_has_header,
                                 uint64_t current_stamp)
{
    if (!incoming_has_header) {
        return false;
    }
    if (!has_current || !current_has_header) {
        return true;
    }
    return is_newer_stamp(incoming_stamp, current_stamp);
}

static void merge_state_if_newer(State *combined, const State *incoming)
{
    /* Keep latest-known value per field using per-field header timestamp freshness. */
    if (incoming->has_effort
        && should_replace_field(incoming->effort.has_header,
                                incoming->effort.header.stamp,
                                combined->has_effort,
                                combined->effort.has_header,
                                combined->effort.header.stamp)) {
        combined->has_effort = true;
        combined->effort = incoming->effort;
    }

    if (incoming->has_linear_acceleration
        && should_replace_field(incoming->linear_acceleration.has_header,
                                incoming->linear_acceleration.header.stamp,
                                combined->has_linear_acceleration,
                                combined->linear_acceleration.has_header,
                                combined->linear_acceleration.header.stamp)) {
        combined->has_linear_acceleration = true;
        combined->linear_acceleration = incoming->linear_acceleration;
    }

    if (incoming->has_angular_velocity
        && should_replace_field(incoming->angular_velocity.has_header,
                                incoming->angular_velocity.header.stamp,
                                combined->has_angular_velocity,
                                combined->angular_velocity.has_header,
                                combined->angular_velocity.header.stamp)) {
        combined->has_angular_velocity = true;
        combined->angular_velocity = incoming->angular_velocity;
    }

    if (incoming->has_magnetic_field
        && should_replace_field(incoming->magnetic_field.has_header,
                                incoming->magnetic_field.header.stamp,
                                combined->has_magnetic_field,
                                combined->magnetic_field.has_header,
                                combined->magnetic_field.header.stamp)) {
        combined->has_magnetic_field = true;
        combined->magnetic_field = incoming->magnetic_field;
    }

    if (incoming->has_distance
        && should_replace_field(incoming->distance.has_header,
                                incoming->distance.header.stamp,
                                combined->has_distance,
                                combined->distance.has_header,
                                combined->distance.header.stamp)) {
        combined->has_distance = true;
        combined->distance = incoming->distance;
    }
}

void state_periodic_publisher_task(void)
{
    const struct zbus_channel *chan;
    State combined = State_init_zero;

    int64_t next_publish_at_ms = k_uptime_get();

    LOG_INF("State periodic publisher active: %d ms", CONFIG_KABOT_STATE_EGRESS_PERIOD_MS);

    while (true) {
        int64_t now_ms = k_uptime_get();
        int64_t wait_ms = next_publish_at_ms - now_ms;
        if (wait_ms < 0) {
            wait_ms = 0;
        }

        int rc = -EAGAIN;
        if (wait_ms > 0) {
            rc = zbus_sub_wait(&state_periodic_publisher, &chan, K_MSEC(wait_ms));
        }

        if ((rc == 0) && (&state_channel == chan)) {
            State incoming;
            if (zbus_chan_read(&state_channel, &incoming, K_MSEC(READ_TIMEOUT_MS)) != 0) {
                LOG_WRN("Failed to read state_channel");
            } else {
                merge_state_if_newer(&combined, &incoming);
            }
        } else if ((rc != 0) && (rc != -EAGAIN) && (rc != -ETIMEDOUT)) {
            LOG_WRN("state_periodic_publisher wait error: %d", rc);
        }

        now_ms = k_uptime_get();
        if (now_ms < next_publish_at_ms) {
            continue;
        }

        next_publish_at_ms += CONFIG_KABOT_STATE_EGRESS_PERIOD_MS;
        if (next_publish_at_ms <= now_ms) {
            next_publish_at_ms = now_ms + CONFIG_KABOT_STATE_EGRESS_PERIOD_MS;
        }

        State to_send = combined;
        to_send.has_header = true;
        to_send.header.stamp = (uint64_t)now_ms;

        rc = publish_state_egress_msg(&to_send, K_MSEC(PUBLISH_TIMEOUT_MS));
        if (rc != 0) {
            LOG_WRN("Failed to publish state_egress message: %d", rc);
        }
    }
}

K_THREAD_DEFINE(state_periodic_publisher_task_id,
                CONFIG_MAIN_STACK_SIZE,
                state_periodic_publisher_task,
                NULL,
                NULL,
                NULL,
                4,
                0,
                0);
