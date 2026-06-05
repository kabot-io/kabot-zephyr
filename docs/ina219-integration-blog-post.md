# INA219 Telemetry in Kabot: Left, Right, and Supply Power Visibility

Date: June 5, 2026

Adding INA219 support was not just a driver hookup. We needed the full stack to agree on data shape and cadence: devicetree aliases, firmware publishers, state aggregation, protobuf schema, and HMI visualization.

This post summarizes what changed and why.

## Why INA219, and Why Three Sensors

The goal was to observe power behavior at three points:

- left branch
- right branch
- supply branch

Using three INA219 devices gives per-branch current, bus voltage, and power in a way that is easy to stream through the existing machine-mirror state architecture.

## Addressing Plan on Shared I2C

All three INA219 devices are on the same bus as the magnetometer (`i2c0`).
Address selection is set by INA219 A0/A1 strap combinations:

- left: A0=GND, A1=GND -> `0x40`
- right: A0=SDA, A1=GND -> `0x42`
- supply: A0=SDA, A1=SDA -> `0x4a`

Board aliases were added to make publisher binding explicit:

- `kabot-current-left`
- `kabot-current-right`
- `kabot-current-supply`

## Firmware Path: Producer -> Aggregator -> Egress

The implementation follows the established zbus state flow:

1. `current_publisher` reads each INA219.
2. It publishes partial `State` fragments with stamps/frame IDs.
3. `state_aggregator` merges field-wise by timestamp.
4. periodic egress publishes complete snapshots.

Published channels per sensor:

- `SENSOR_CHAN_CURRENT`
- `SENSOR_CHAN_VOLTAGE`
- `SENSOR_CHAN_POWER`

Cadence:

- `20 ms` period (`50 Hz`) via `CONFIG_KABOT_STATE_CURRENT_PERIOD_MS`.

## Simulated Publisher for Development

A simulated current publisher was added so native simulation and bring-up workflows can exercise the same schema and HMI views without hardware.

It emits realistic positive values for:

- current
- bus voltage
- power

This keeps contract testing available even when hardware is disconnected.

## Protobuf Contract Extension

`State` now includes nine INA219 scalar fields:

- left: current, bus voltage, power
- right: current, bus voltage, power
- supply: current, bus voltage, power

Because protobuf fields are additive, older decoders can ignore unknown fields while updated tooling can immediately display them.

## HMI Improvements from Real Usage

As soon as INA219 telemetry was plotted, two usability gaps appeared:

1. value ranges were too broad for useful detail
2. bottom plots were not visible without scrolling

So the HMI was updated to:

- set left/right current axis to `0 .. 0.5`
- set left/right bus voltage axis to `0 .. 6`
- move supply traces to dedicated plots
- set supply current axis to `0 .. 0.8`
- make the rolling plot pane vertically scrollable
- add an `Enable plots` checkbox so operators can disable plot rendering when
	packet/field update rates exceed host UI rendering capacity

This made the new telemetry practical for live debugging instead of just technically present.

## Lessons Learned

1. Sensor integration is an end-to-end change, not a single-file change.
2. Alias-based devicetree binding scales better than ad-hoc discovery logic.
3. Sim publishers are valuable contract tests, not just placeholders.
4. Plot ergonomics matter as much as transport correctness for day-to-day engineering.

## Next Practical Step

Tune INA219 calibration (`shunt-milliohm`, `lsb-microamp`) against measured hardware references so current and power values are not only stable, but physically accurate.