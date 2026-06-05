# INA219 Triple Current Sensor Implementation Report

Date: June 5, 2026

## Summary

Implemented support for three INA219 current sensors and integrated telemetry through firmware and host HMI.

Delivered:

- board devicetree aliases and INA219 nodes on the magnetometer bus (`i2c0`)
- real publisher thread for left/right/supply INA219 devices
- simulated publisher for INA219-like current/voltage/power streams
- protobuf `State` extension for 9 INA219 scalar fields
- state aggregator merge support for all new fields
- host decode/state panel/plot updates
- HMI rolling-plot panel scrolling support for expanded plot count
- HMI `Enable plots` checkbox to disable plotting load when host rendering falls behind

## Hardware Mapping

Configured on `i2c0`:

- left INA219 (`kabot-current-left`) at `0x40` (A0=GND, A1=GND)
- right INA219 (`kabot-current-right`) at `0x42` (A0=SDA, A1=GND)
- supply INA219 (`kabot-current-supply`) at `0x4a` (A0=SDA, A1=SDA)

Aliases added in board overlay:

- `kabot-current-left`
- `kabot-current-right`
- `kabot-current-supply`

## Firmware Changes

### Protobuf state contract

Extended `State` with:

- `current_left`, `bus_voltage_left`, `power_left`
- `current_right`, `bus_voltage_right`, `power_right`
- `current_supply`, `bus_voltage_supply`, `power_supply`

### Real publisher

Added:

- `app/src/zbus/state/current_publisher.c`

Behavior:

- binds via aliases (`kabot-current-left/right/supply`)
- startup readiness retry loop using `CONFIG_KABOT_SENSOR_RETRY_PERIOD_MS`
- periodic fetch/get of:
  - `SENSOR_CHAN_CURRENT`
  - `SENSOR_CHAN_VOLTAGE`
  - `SENSOR_CHAN_POWER`
- publishes partial `State` at `CONFIG_KABOT_STATE_CURRENT_PERIOD_MS` (default `20 ms`, 50 Hz)

### Simulated publisher

Added:

- `app/src/zbus/state/sim_current_publisher.c`

Wired with:

- `CONFIG_KABOT_ENABLE_SIMULATED_CURRENT_PUBLISHER`
- CMake include in simulated-sensor source block

Ranges aligned to HMI scale targets:

- current: `0.0 .. 0.5`
- bus voltage: `0.0 .. 6.0`
- power: constrained positive range

### Kconfig/CMake/board wiring

Updated:

- `app/Kconfig`
  - real current publisher symbols
  - simulated current publisher symbol
  - simulated sensors help text
- `app/CMakeLists.txt`
  - source gating for real and simulated current publishers
- `app/boards/esp32s3_devkitc_esp32s3_procpu.conf`
  - enabled real current publisher
  - disabled simulated current publisher on ESP32S3 board

### Aggregator

Updated `app/src/zbus/state/state_aggregator.c` to merge all 9 INA219 fields with existing timestamp newer-or-equal policy.

## Host/HMI Changes

Updated:

- `scripts/kabot_io/model.py`
- `scripts/kabot_io/state_fields.py`
- `scripts/kabot_io/view.py`

Behavior updates:

- all INA219 fields decode and display in state panel
- left/right plot ranges:
  - current: `0 .. 0.5`
  - bus voltage: `0 .. 6`
- supply moved to separate dedicated plots
- supply current plot range updated to `0 .. 0.8`
- rolling plots pane is now vertically scrollable
- rolling plots can be disabled from UI (`Enable plots`) without affecting state-field updates

## Additional Bus Tuning

- Distance sensor bus (`i2c1` for VL53L0X) was updated from standard-mode to
  fast-mode (`400 kHz`) to improve transaction throughput.

## Validation

Completed:

- ESP32S3 build completed successfully (`./scripts/build.zsh --no-flash`)
- generated devicetree confirms aliases and addresses for `0x40`, `0x42`, `0x4a`

Pending hardware checks:

1. verify each INA219 channel reports expected values under known load
2. verify left/right/supply diverge correctly under asymmetric load conditions
3. verify expected publish cadence (~50 Hz) in HMI `*_header_hz` fields
4. confirm shunt calibration (`shunt-milliohm`, `lsb-microamp`) against actual hardware BOM

## Notes

INA219 measurement accuracy depends on correct shunt and LSB calibration values. Current defaults are practical placeholders and should be tuned to board-specific shunt resistors for production-grade telemetry.