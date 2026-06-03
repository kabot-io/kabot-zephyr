# Dual LTR329 Light Sensor Implementation Report

Date: June 4, 2026

## Summary

Implemented support for two LTR329 ambient light sensors behind a TCA9546A I2C mux on the main i2c0 bus. Integration includes:

- board devicetree mux + channel buses + per-channel LTR329 nodes
- real firmware publisher for both sensors
- protobuf State extension with `light_left` and `light_right`
- state aggregator merge support
- host decode/display/plot updates
- LTR55x driver lux arithmetic hardening in local Zephyr tree

## Hardware Topology

- TCA9546A root on i2c0 at `0x70`
- LTR329 left on mux channel 0 at `0x29`
- LTR329 right on mux channel 1 at `0x29`

This enables address reuse while keeping each sensor isolated on a different mux channel bus.

## Firmware Changes

### Devicetree and board config

- Updated [app/boards/esp32s3_devkitc_esp32s3_procpu.overlay](app/boards/esp32s3_devkitc_esp32s3_procpu.overlay)
  - added TCA9546A root node
  - added `mux_i2c0@0` and `mux_i2c1@1`
  - added `ltr329_left` and `ltr329_right`
  - added aliases `kabot-light-left` and `kabot-light-right`

- Updated [app/boards/esp32s3_devkitc_esp32s3_procpu.conf](app/boards/esp32s3_devkitc_esp32s3_procpu.conf)
  - enabled `CONFIG_I2C_TCA954X`
  - enabled `CONFIG_LTR55X`
  - set init ordering:
    - `CONFIG_I2C_TCA954X_ROOT_INIT_PRIO=50`
    - `CONFIG_I2C_TCA954X_CHANNEL_INIT_PRIO=51`

### State and publisher path

- Updated [app/protos/state_control_msg.proto](app/protos/state_control_msg.proto)
  - `StateScalar light_left = 7;`
  - `StateScalar light_right = 8;`

- Added [app/src/zbus/state/light_publisher.c](app/src/zbus/state/light_publisher.c)
  - alias-based device binding for left/right sensors
  - startup retry loop using `CONFIG_KABOT_SENSOR_RETRY_PERIOD_MS`
  - periodic fetch/get on `SENSOR_CHAN_LIGHT`
  - publishes any valid subset of `light_left` and `light_right` each cycle
  - skips invalid conversion samples (`-EINVAL`) to avoid value spikes

- Updated [app/src/zbus/state/state_aggregator.c](app/src/zbus/state/state_aggregator.c)
  - timestamp merge handling for `light_left` and `light_right`

- Updated [app/Kconfig](app/Kconfig)
  - `KABOT_ENABLE_LIGHT_PUBLISHER`
  - `KABOT_STATE_LIGHT_PERIOD_MS`
  - `KABOT_STATE_LIGHT_LEFT_FRAME_ID`
  - `KABOT_STATE_LIGHT_RIGHT_FRAME_ID`
  - `KABOT_LIGHT_PUBLISHER_STACK_SIZE`

- Updated [app/CMakeLists.txt](app/CMakeLists.txt)
  - conditional source inclusion for `light_publisher.c`

## Zephyr Driver Hardening

Patched [deps/zephyr/drivers/sensor/liteon/ltr55x/ltr55x.c](deps/zephyr/drivers/sensor/liteon/ltr55x/ltr55x.c):

- switched lux intermediate math to signed `int64_t` in all branches
- preserved existing coefficient and threshold logic
- clamped negative microlux to zero
- converted using `sensor_value_from_micro()`

This removes unsigned underflow artifacts in the subtractive branch.

## Host/HMI Changes

- Updated [scripts/kabot_io/model.py](scripts/kabot_io/model.py)
  - decode and store `light_left` and `light_right`

- Updated [scripts/kabot_io/state_fields.py](scripts/kabot_io/state_fields.py)
  - field mappings and hz tracking for both light channels

- Updated [scripts/kabot_io/view.py](scripts/kabot_io/view.py)
  - added light plot with left/right traces

## Validation Snapshot

- Build gate passed using project build script:
  - `source .venv/bin/activate && ./scripts/build.zsh --no-flash`

- Host syntax gate passed:
  - `python -m py_compile scripts/kabot_io/model.py scripts/kabot_io/state_fields.py scripts/kabot_io/view.py`

- Generated devicetree confirmed separate bus lineage:
  - `ltr329_left` -> `...mux_i2c0_0`
  - `ltr329_right` -> `...mux_i2c1_1`

## Remaining Runtime Validation

Hardware runtime checks are still required for final sign-off:

1. `sensor get` or equivalent fetch/get for each light sensor under asymmetric lighting.
2. Optional channel-specific raw I2C reads for debug parity.
3. Verify both light fields appear in state stream over time at expected cadence.
4. Verify invalid conversion cycles are skipped and do not create spike values.
