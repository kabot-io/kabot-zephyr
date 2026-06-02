#pragma once

#include "protos/state_control_msg.pb.h"

void state_aggregator_merge_update(const State *incoming);
void state_aggregator_get_snapshot(State *out);
