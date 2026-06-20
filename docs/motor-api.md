# Motor API

## Overview

The motor subsystem provides a Zephyr device API for DC motor effort control.

- API class name: `motor`
- Public header: [modules/motor_driver/include/motor/motor_driver.h](../modules/motor_driver/include/motor/motor_driver.h)
- Effort type: signed Q31 (`int32_t`), where:
  - `INT32_MIN` is full reverse
  - `0` is neutral
  - `INT32_MAX` is full forward

This follows the same model used by other Zephyr driver APIs:

- backend drivers register devices with `DEVICE_DT_DEFINE(...)`
- users operate on `const struct device *`
- API calls dispatch through `DEVICE_API_GET(motor, dev)`

## Public Contract

Defined in [modules/motor_driver/include/motor/motor_driver.h](../modules/motor_driver/include/motor/motor_driver.h):

- `struct motor_driver_api`
  - `set_effort(const struct device *dev, int32_t effort_q31)`
- `__syscall int motor_set_effort(const struct device *dev, int32_t effort_q31)`
- `z_impl_motor_set_effort(...)` implementation used by the syscall layer

Wrapper behavior:

- asserts `dev` and API function pointer are valid
- validates Q31 effort input
- returns backend error code directly

Syscall integration:

- header includes generated syscall declarations at end of file
- module registers syscall include scanning in build system via
  `zephyr_syscall_include_directories(include)`

## Effort Representation

Helpers are defined in [modules/motor_driver/include/motor/motor_math.h](../modules/motor_driver/include/motor/motor_math.h):

- `motor_percent_to_q31(...)`
- `motor_q31_to_percent(...)`
- constants:
  - `MOTOR_EFFORT_Q31_MIN`
  - `MOTOR_EFFORT_Q31_MAX`
  - `MOTOR_EFFORT_PERCENT_MIN`
  - `MOTOR_EFFORT_PERCENT_MAX`

Usage policy in app code:

- shell and UDP ingress use percent-friendly UX/protocol
- they convert to Q31 before invoking the motor API
- all backend drivers consume Q31

## Devicetree Integration

Current compatibles:

- `kabot,esc`
- `kabot,h-bridge`
- `kabot,sim-motor`

Bindings:

- [modules/motor_driver/dts/bindings/kabot,esc.yaml](../modules/motor_driver/dts/bindings/kabot,esc.yaml)
- [modules/motor_driver/dts/bindings/kabot,h-bridge.yaml](../modules/motor_driver/dts/bindings/kabot,h-bridge.yaml)
- [modules/motor_driver/dts/bindings/kabot,sim-motor.yaml](../modules/motor_driver/dts/bindings/kabot,sim-motor.yaml)

Alias usage:

- `motor-left`
- `motor-right`

The effort subscriber uses these aliases to route left/right effort messages.

## Backend Inventory

Current motor-compatible backends:

- ESC backend: [modules/motor_driver/drivers/motor/esc_driver.c](../modules/motor_driver/drivers/motor/esc_driver.c)
- H-bridge backend: [modules/motor_driver/drivers/motor/h_bridge_driver.c](../modules/motor_driver/drivers/motor/h_bridge_driver.c)
- Sim backend: [modules/motor_driver/drivers/motor/sim_motor_driver.c](../modules/motor_driver/drivers/motor/sim_motor_driver.c)

Both register Zephyr devices and provide `struct motor_driver_api` with `set_effort` implemented.

## Build Wiring

Motor driver sources are grouped under [modules/motor_driver/drivers/motor](../modules/motor_driver/drivers/motor) in module build:

- [modules/motor_driver/CMakeLists.txt](../modules/motor_driver/CMakeLists.txt)

The module also registers include path for syscall/subsystem generation in:

- [modules/motor_driver/CMakeLists.txt](../modules/motor_driver/CMakeLists.txt)

The app enables this module using:

- [app/CMakeLists.txt](../app/CMakeLists.txt)

Messaging sources are grouped under [app/src/zbus](../app/src/zbus) in:

- [app/CMakeLists.txt](../app/CMakeLists.txt)

This keeps motor driver code isolated from app message transport code.

## See Also

- [motor-app-usage.md](motor-app-usage.md)
- [motor-driver-development.md](motor-driver-development.md)
- [README.md](README.md)
- [README.md](../README.md)
