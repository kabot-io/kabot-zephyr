# Dual Light Sensors Behind TCA9546A: What Changed and Why

Date: June 4, 2026

Adding one ambient light sensor is straightforward. Adding two of the same sensor at the same I2C address requires bus isolation and careful integration across firmware and host telemetry.

This post summarizes how dual LTR329 support was integrated in kabot firmware using a TCA9546A mux.

## Problem

Both LTR329 sensors use address `0x29`. On one shared bus, that collides immediately. We also needed both readings to propagate through the existing machine-mirror State pipeline and appear in the host HMI.

## Design Choices

1. Use TCA9546A mux channels to isolate each `0x29` sensor.
2. Keep publisher architecture aligned with existing real sensor pattern:
- alias-based binding
- `sensor_sample_fetch()` + `sensor_channel_get()`
- periodic partial-State publish
3. Represent measurements as explicit scalar fields:
- `State.light_left`
- `State.light_right`
4. Harden driver lux math where subtractive branch could underflow with unsigned arithmetic.

## Firmware Implementation

### Devicetree

In [app/boards/esp32s3_devkitc_esp32s3_procpu.overlay](app/boards/esp32s3_devkitc_esp32s3_procpu.overlay):

- added TCA9546A root on `i2c0`
- added channel buses `mux_i2c0@0` and `mux_i2c1@1`
- placed one LTR329 per channel
- added aliases:
  - `kabot-light-left`
  - `kabot-light-right`

### Kconfig/CMake and Publisher

- new publisher options in [app/Kconfig](app/Kconfig)
- source gating in [app/CMakeLists.txt](app/CMakeLists.txt)
- new dual-light worker in [app/src/zbus/state/light_publisher.c](app/src/zbus/state/light_publisher.c)

The publisher reads both devices in one loop and publishes one partial State fragment
containing whichever light fields are valid in that cycle.

If conversion returns an invalid sample (`-EINVAL`) for one side, that side is skipped for
the cycle so no fallback spike value is injected.

### State Schema and Aggregation

- extended [app/protos/state_control_msg.proto](app/protos/state_control_msg.proto) with `light_left` and `light_right`
- extended merge logic in [app/src/zbus/state/state_aggregator.c](app/src/zbus/state/state_aggregator.c)

This keeps light telemetry in the same timestamp-based merge model as other sensors.

## Host Integration

Updated:

- [scripts/kabot_io/model.py](scripts/kabot_io/model.py)
- [scripts/kabot_io/state_fields.py](scripts/kabot_io/state_fields.py)
- [scripts/kabot_io/view.py](scripts/kabot_io/view.py)

Result: both channels decode, display, and plot in HMI.

## Driver Safety Fix

In local Zephyr tree [deps/zephyr/drivers/sensor/liteon/ltr55x/ltr55x.c](deps/zephyr/drivers/sensor/liteon/ltr55x/ltr55x.c):

- switched lux intermediate math to signed arithmetic
- clamped negative micro-lux to zero
- used `sensor_value_from_micro()` conversion

This eliminates unsigned underflow artifacts in subtractive lux branch calculations.

## Validation

Completed:

- project build gate (`./scripts/build.zsh --no-flash`) passed
- host Python syntax gate passed
- generated devicetree confirms distinct bus lineage for left/right sensors

Pending on-device checks:

1. verify left/right differentiation under asymmetric illumination
2. verify both fields appear in live telemetry stream over time
3. verify invalid conversion cycles are skipped without spike injection

## Takeaways

1. Mux child-bus declaration details matter as much as root compatibility.
2. Alias-based binding keeps real publishers predictable.
3. Schema and host updates should be done in the same change set to avoid silent telemetry gaps.
4. Signed intermediate math is safer for subtractive sensor formulas.
