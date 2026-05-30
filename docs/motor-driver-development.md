# Motor Driver Development Guide

## Goal

This guide explains how to implement a new motor backend that is compatible with the app motor API.

Current structure in this repository:

- motor driver module: `modules/motor_driver`
- app motor UDP service: `app/src/motor`
- module motor shell: `modules/motor_driver/subsys/motor`
- app zbus transport/subscriber: `app/src/zbus`

## Requirements

A compatible driver must:

1. Register one or more Zephyr devices.
2. Expose the `motor` API (`struct motor_driver_api`).
3. Implement `set_effort(const struct device *dev, int32_t effort_q31)`.
4. Define a devicetree binding and compatible string.
5. Use Q31 effort semantics.

## Step 1: Add Devicetree Binding

Create a binding file in `modules/motor_driver/dts/bindings`, for example:

- `vendor,my-motor.yaml`

Include:

- `compatible: "vendor,my-motor"`
- required properties for your hardware (PWM, GPIOs, inversion, limits, etc.)

## Step 2: Add Driver Config/Data Types

Create a header under `modules/motor_driver/include/motor`, for example:

- `modules/motor_driver/include/motor/my_motor_driver.h`

Define config/data structs used by `dev->config` and/or `dev->data`.

## Step 3: Implement Backend Source

Create source under `modules/motor_driver/drivers/motor`, for example:

- `modules/motor_driver/drivers/motor/my_motor_driver.c`

Typical pattern:

1. include `motor/motor_driver.h`
2. parse config from DT macros
3. implement `my_motor_set_effort(...)`
4. define `static const struct motor_driver_api my_motor_api = { ... }`
5. instantiate devices with `DEVICE_DT_DEFINE(...)`

## Step 4: Map Q31 to Hardware Command

Your `set_effort` implementation should:

1. treat input as signed Q31 (`INT32_MIN..INT32_MAX`)
2. map sign to direction
3. map magnitude to actuator command
4. keep math overflow-safe (use 64-bit intermediates where needed)

Recommendation:

- convert to backend-native range with bounded arithmetic
- avoid floating-point when fixed-point is sufficient

## Step 5: Error Semantics

Return standard negative errno values on failures, for example:

- `-EINVAL` invalid config/input
- `-ENODEV` required HW not ready
- `-EIO` HW operation failed

The API wrapper passes these codes to callers unchanged.

## Step 6: Expose Instances in Devicetree

Add nodes to board overlay(s), e.g.:

- `app/boards/esp32s3_devkitc_esp32s3_procpu.overlay`
- `app/boards/native_sim.overlay`

If this backend should participate in left/right app control, point aliases:

- `motor-left = &my_left_node;`
- `motor-right = &my_right_node;`

## Step 7: Update Shell Discovery List

The shell currently lists motor devices from known motor-compatible DT nodes.

If you add a new compatible, update the compatible list in:

- `modules/motor_driver/subsys/motor/motor_shell.c`

Specifically extend the `motor_names` macro expansion with your compatible.

## Step 8: Build Integration

Add source to `modules/motor_driver/CMakeLists.txt` under module driver sources.

The app integrates the module in `app/CMakeLists.txt` using `ZEPHYR_EXTRA_MODULES`.

## Validation Checklist

1. Build passes on target board.
2. Device is present in `motor list` output.
3. `motor set <name> 0` drives neutral.
4. `motor set <name> 50` and `-50` produce expected direction/magnitude.
5. Effort subscriber drives alias-bound left/right devices.

## Reference Implementations

Use existing drivers as templates:

- `modules/motor_driver/drivers/motor/esc_driver.c`
- `modules/motor_driver/drivers/motor/h_bridge_driver.c`
- `modules/motor_driver/drivers/motor/sim_motor_driver.c`
