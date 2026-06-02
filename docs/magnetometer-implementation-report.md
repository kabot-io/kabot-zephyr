# MMC5603 Magnetometer Bring-Up Report

## Why This Report Exists

This is a practical implementation report for adding MMC5603 magnetometer support to kabot firmware.

It follows the architecture in the real sensor tutorial and records:

- what was implemented
- what was changed in code and devicetree
- what broke during bring-up
- which debugging tools and commands were used
- which prompting patterns were successful during implementation/debugging
- what remains to validate on hardware

## Architecture Baseline

The implementation follows the state egress architecture from the real sensor tutorial:

- sensor publisher thread reads hardware through Zephyr sensor API
- publisher emits partial `State` fragments to `state_channel`
- aggregator merges by per-field timestamps
- periodic publisher emits full machine-mirror state snapshot

Reference:

- `docs/real-sensor-publisher-tutorial.md`
- `docs/firmware-data-flow.md`

## Implementation Summary

### 1. Real Magnetometer Publisher Added

A new real publisher thread was added:

- `app/src/zbus/state/magnetometer_publisher.c`

Behavior:

- resolves magnetometer from devicetree (`memsic,mmc56x3`)
- checks `device_is_ready`
- reads with `sensor_sample_fetch` + `sensor_channel_get(SENSOR_CHAN_MAGN_XYZ)`
- fills `State.magnetic_field` with stamp and frame id
- publishes to `state_channel` at `CONFIG_KABOT_STATE_MAG_PERIOD_MS`

### 2. Build and Kconfig Wiring

A new Kconfig gate for real magnetometer publishing was added:

- `app/Kconfig`: `CONFIG_KABOT_ENABLE_MAGNETOMETER_PUBLISHER`

Build wiring was added:

- `app/CMakeLists.txt` includes `magnetometer_publisher.c` when enabled

Board configuration updates:

- `app/boards/esp32s3_devkitc_esp32s3_procpu.conf`
- simulated magnetometer disabled
- real magnetometer publisher enabled

### 3. Devicetree Integration

MMC5603 node was added on I2C0:

- `app/boards/esp32s3_devkitc_esp32s3_procpu.overlay`
- node: `mmc5603@30`
- compatible: `memsic,mmc56x3`
- address: `0x30`

VL53L0X was moved to I2C1 (kept on `0x29`) to split buses as intended.

## Problems Faced During Bring-Up

### Problem A: Compile Error in Publisher Helper

Symptom:

- `redefinition of sensor_value_to_float`

Cause:

- local helper in publisher had the same name as Zephyr inline helper in `sensor.h`

Fix:

- renamed local helper to avoid collision

### Problem B: MMC56X3 Driver Build Failure Due to Missing DT Properties

Symptom:

- generated DT macro for `magn_odr` missing

Cause:

- MMC56X3 driver expects specific DT properties to exist

Fix:

- add required properties on `mmc5603@30` node:
  - `magn-odr = <0>;`
  - `auto-self-reset;`

### Problem C: VL53L0X I2C Failures After Bus Move

Symptom:

- repeated runtime logs like `i2c_reg_write_byte failed (-20)`

Cause:

- I2C1 default pins (GPIO4/GPIO5) conflicted with PCNT encoder pin assignment in board overlay

Fix:

- created custom I2C1 pinctrl for TOF
- moved I2C1 TOF bus to GPIO40 (SDA) and GPIO39 (SCL)

Result:

- TOF device became visible on I2C1 scan at address `0x29`

### Problem D: I2C0 Magnetometer Not Found

Symptom:

- `MMC56X3 device not ready: mmc5603@30`
- `i2c scan i2c0` found 0 devices

Cause found in software:

- initial pin expectation mismatch for I2C0 line assignment versus hardware statement

Fix applied:

- explicit I2C0 pinctrl override for hardware mapping:
  - SCL -> GPIO1
  - SDA -> GPIO2

Current status:

- compiled DTS confirms I2C0 uses custom mapping and includes `mmc5603@30`
- runtime scan still reports no devices on I2C0
- this points to likely electrical/hardware path issues (power, wiring, pull-ups, address strap)

Post-review hardening applied:

- startup readiness is now retried in a loop instead of terminating the publisher thread
- retry cadence is 1 second between checks
- this avoids a permanent telemetry outage from transient boot-time I2C or power-up timing issues

## Debugging Tools and Workflow Used

### 1. Build and Flash Tasks

Used VS Code tasks repeatedly:

- Build esp32s3
- Build and flash esp32s3

Purpose:

- fast validation after each overlay/Kconfig/publisher change

### 2. Runtime Logging

Used boot and runtime logs to identify failure class:

- publisher readiness errors
- sensor driver error logs
- VL53L0X transaction failures

Purpose:

- distinguish compile-time, init-time, and runtime bus faults

### 3. I2C Shell

Enabled and used Zephyr I2C shell:

- `i2c scan i2c0`
- `i2c scan i2c1`
- `i2c recover <device>` available for bus recovery

Purpose:

- verify physical bus visibility independent of app-level publisher logic

Observed:

- I2C1 detects `0x29` (VL53L0X)
- I2C0 detects nothing

### 4. Generated Devicetree Inspection

Inspected generated DTS:

- `build/esp32s3_devkitc/zephyr/zephyr.dts`

Purpose:

- confirm effective pinctrl and node placement after overlays
- avoid guessing from source overlays alone

### 5. Zephyr Source Inspection

Inspected local Zephyr sources for binding/driver expectations:

- `deps/zephyr/dts/bindings/sensor/memsic,mmc56x3.yaml`
- `deps/zephyr/drivers/sensor/memsic/mmc56x3/mmc56x3.c`
- `deps/zephyr/drivers/i2c/i2c_shell.c`

Purpose:

- validate compatible string, required DT properties, and shell command syntax

## What Was Used (At a Glance)

- Zephyr sensor API (`sensor_sample_fetch`, `sensor_channel_get`)
- zbus state publisher pattern from existing distance publisher
- Kconfig/CMake feature gating
- Devicetree overlays and custom pinctrl groups
- Zephyr I2C shell for runtime bus scans
- Generated DTS inspection for effective config confirmation

## Prompts That Were Used and Worked Well

This bring-up used short, high-signal prompts that constrained hardware topology first,
then narrowed the debugging scope with concrete runtime evidence.

Successful prompt examples:

1. Implementation constraint prompt

- "add support for mmc5603 magnetometer connected to i2c0 on gpios 1 (SCL) and 2 (SDA). i2c0 is the main bus for internal sensors, tof is on i2c1"

Why it worked:

- gave bus ownership constraints up front
- reduced architecture ambiguity before coding

2. Runtime error injection prompt

- posting repeated VL53L0X driver errors after flashing

Why it worked:

- provided hard evidence that immediately pointed to bus/pin conflict analysis
- let implementation move from assumptions to concrete failure triage

3. Debug capability request prompt

- "add i2c shell so i can scan the bus"

Why it worked:

- added a direct observability tool in firmware
- enabled quick isolation between software integration and electrical presence

4. Wiring verification prompt

- "is it really connected to gpio1 as SCL and to GPIO2 as SDA?"

Why it worked:

- forced validation against generated DTS (effective config) rather than source-only assumptions
- revealed and corrected a real SDA/SCL expectation mismatch during bring-up

5. Evidence-driven scan output prompt

- posting both scan outputs where I2C1 found `0x29` and I2C0 found none

Why it worked:

- confirmed TOF bus path was healthy
- narrowed magnetometer issue to I2C0 hardware visibility/electrical path

Prompting patterns that were most effective in this session:

1. Start with explicit hardware constraints (bus, pins, ownership).
2. Follow with exact runtime logs, not summaries.
3. Ask for an observability feature (shell/scan) when visibility is low.
4. Validate assumptions by requesting generated artifact checks (DTS, config, map).
5. Iterate with concrete command output after each firmware change.

## Lessons Learned

1. Always inspect generated DTS, not just overlay source.
2. ESP32S3 bus pin defaults can conflict with other peripherals (PCNT here).
3. MMC56X3 driver expects DT properties that are easy to miss.
4. Shell-level I2C scans quickly separate software integration bugs from hardware presence issues.
5. Migrate one sensor path at a time and keep simulation toggles explicit during transition.
6. Do not terminate publisher threads on first `device_is_ready()` failure;
  use bounded-log retry loops so delayed hardware init can recover automatically.

## Current Outcome

- Real magnetometer publisher path is implemented and integrated.
- TOF bus split and I2C1 conflict issue is resolved.
- I2C shell tooling is enabled for on-device diagnostics.
- Remaining blocker is hardware-level visibility of MMC5603 on I2C0.

## Suggested Next Validation Steps

1. Verify MMC5603 power and shared ground physically.
2. Verify pull-ups on I2C0 SDA/SCL lines.
3. Confirm MMC5603 address strap (0x30 vs 0x31) and scan both expectations.
4. Probe I2C0 lines during scan with scope/logic analyzer.
5. If hardware checks pass, add temporary low-level I2C probe logs for detailed error code diagnostics.
