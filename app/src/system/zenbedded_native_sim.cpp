#include <math.h>
#include <zenbedded/rcl/zenbedded_client.hpp>
#include <zenbedded_transport/generated/interface_data.h>

#include "system/wheel_encoder_state.h"

extern "C" {
#include "system/robot_settings.h"
#include "zbus/channels/control_channel.h"
#include "zbus/channels/sensor_channel.h"
}

using NativeSimClient =
  ZenbeddedClient<RawCodec<zenbedded_state_t>, RawCodec<zenbedded_command_t>>;

static NativeSimClient client;
static uint32_t last_command_count;

static float encoder_position(const sensor_q31_data &encoder)
{
  constexpr float degrees_to_radians = 0.017453292519943295f;
  return ldexpf(static_cast<float>(encoder.readings[0].value), encoder.shift - 31) *
         degrees_to_radians;
}

extern "C" int zenbedded_native_sim_init(void)
{
  return client.init(10);
}

extern "C" void zenbedded_native_sim_update(void)
{
  static zenbedded_state_t state{};
  static WheelEncoderState left_encoder;
  static WheelEncoderState right_encoder;
  sensor_msg sensor{};
  if (zbus_chan_read(&sensor_channel, &sensor, K_NO_WAIT) == 0 &&
      sensor.left_encoder.header.reading_count > 0 &&
      sensor.right_encoder.header.reading_count > 0) {
    left_encoder.update(encoder_position(sensor.left_encoder),
                        sensor.left_encoder.header.base_timestamp_ns);
    right_encoder.update(encoder_position(sensor.right_encoder),
                         sensor.right_encoder.header.base_timestamp_ns);
    state.left_wheel_joint_position = left_encoder.position;
    state.left_wheel_joint_velocity = left_encoder.velocity;
    state.right_wheel_joint_position = right_encoder.position;
    state.right_wheel_joint_velocity = right_encoder.velocity;
  }
  state.kabot_heartbeat = static_cast<float>(k_uptime_get()) / 1000.0f;
  client.write_state(state);

  const uint32_t command_count = client.accepted_command_count();
  if (command_count == last_command_count) {
    return;
  }
  last_command_count = command_count;
  zenbedded_command_t command{};
  if (!client.read_command(command) || !robot_settings_is_claimed()) {
    return;
  }

  Control control = Control_init_zero;
  control.effort.state.x = command.left_wheel_joint_effort;
  control.effort.state.y = command.right_wheel_joint_effort;
  (void)publish_control_msg(&control, K_NO_WAIT);
}