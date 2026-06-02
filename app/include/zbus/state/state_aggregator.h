#pragma once

#include "protos/state_control_msg.pb.h"

/**
 * @brief Merge an incoming partial state update into the aggregated state.
 *
 * @param incoming Pointer to the incoming State message.
 */
void state_aggregator_merge_update(const State *incoming);

/**
 * @brief Get a snapshot of the current aggregated state.
 *
 * @param out Pointer to the State structure where the snapshot will be copied.
 */
void state_aggregator_get_snapshot(State *out);
