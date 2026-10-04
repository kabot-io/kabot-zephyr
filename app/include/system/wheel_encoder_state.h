#pragma once

#include <math.h>
#include <stdint.h>

struct WheelEncoderState {
  float position = 0.0f;
  float velocity = 0.0f;
  uint64_t timestamp_ns = 0;
  bool initialized = false;

  void update(float next_position, uint64_t next_timestamp_ns)
  {
    if (initialized && next_timestamp_ns == timestamp_ns) {
      return;
    }

    velocity = 0.0f;
    if (initialized && next_timestamp_ns > timestamp_ns) {
      constexpr float full_turn = 6.283185307179586f;
      const float delta = remainderf(next_position - position, full_turn);
      const float elapsed_seconds = static_cast<float>(next_timestamp_ns - timestamp_ns) * 1e-9f;
      velocity = delta / elapsed_seconds;
    }

    position = next_position;
    timestamp_ns = next_timestamp_ns;
    initialized = true;
  }
};