#include "zbus/state/state_aggregator.h"
#include "zbus/channels/state_channel.h"

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(state_aggregator, LOG_LEVEL_DBG);

static struct k_spinlock aggregator_lock;
static State aggregated_state = State_init_zero;

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

    if (incoming->has_light_left
        && should_replace_field(incoming->light_left.has_header,
                                incoming->light_left.header.stamp,
                                combined->has_light_left,
                                combined->light_left.has_header,
                                combined->light_left.header.stamp)) {
        combined->has_light_left = true;
        combined->light_left = incoming->light_left;
    }

    if (incoming->has_light_right
        && should_replace_field(incoming->light_right.has_header,
                                incoming->light_right.header.stamp,
                                combined->has_light_right,
                                combined->light_right.has_header,
                                combined->light_right.header.stamp)) {
        combined->has_light_right = true;
        combined->light_right = incoming->light_right;
    }
}

void state_aggregator_merge_update(const State *incoming)
{
    if (incoming == NULL) {
        return;
    }

    k_spinlock_key_t key = k_spin_lock(&aggregator_lock);
    merge_state_if_newer(&aggregated_state, incoming);
    k_spin_unlock(&aggregator_lock, key);
}

void state_aggregator_get_snapshot(State *out)
{
    if (out == NULL) {
        return;
    }

    k_spinlock_key_t key = k_spin_lock(&aggregator_lock);
    *out = aggregated_state;
    k_spin_unlock(&aggregator_lock, key);
}

static void state_aggregator_listener_cb(const struct zbus_channel *chan)
{
    if (chan != &state_channel) {
        return;
    }

    const State *incoming = zbus_chan_const_msg(chan);
    state_aggregator_merge_update(incoming);
}

ZBUS_LISTENER_DEFINE(state_aggregator_listener, state_aggregator_listener_cb);
