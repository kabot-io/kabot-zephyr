# Motor API Usage In The App

## Overview

Motor control flow in the app is split into three parts:

1. shell command path (`motor list`, `motor set`)
2. UDP motor service ingress
3. effort subscriber applying left/right commands

App-level motor service code lives under:

- `app/src/motor/motor_service.c`
- `app/include/motor`

Driver API/backends/bindings live in the module:

- `modules/motor_driver`

Motor shell now lives in module subsystem path:

- `modules/motor_driver/subsys/motor/motor_shell.c`

Zbus effort transport code lives under:

- `app/src/zbus`
- `app/include/zbus`

## Motor Shell

Source:

- `modules/motor_driver/subsys/motor/motor_shell.c`

Commands:

- `motor list`
  - iterates all registered shell-visible devices
  - filters to motor class devices via `DEVICE_API_IS(motor, dev)`
- `motor set <name> <effort>`
  - `<effort>` is percent in range `[-100, 100]`
  - command converts percent to Q31
  - calls `motor_set_effort(dev, effort_q31)`

Device name resolution:

- looks up Zephyr device by name using `shell_device_get_binding(...)`

## Motor Service (UDP Ingress)

Source:

- `app/src/motor/motor_service.c`

Behavior:

- listens on UDP ports for left/right effort bytes
- interprets payload as signed percent
- converts percent to Q31
- publishes a combined effort message on `effort_channel`

## Effort Channel and Subscriber

Sources:

- `app/src/zbus/effort_channel.c`
- `app/src/zbus/effort_subscriber.c`

Headers:

- `app/include/zbus/effort_msg.h`
- `app/include/zbus/effort_channel.h`
- `app/include/zbus/effort_subscriber.h`

Behavior:

- effort messages carry Q31 left/right values
- subscriber binds directly to DT aliases:
  - `motor-left`
  - `motor-right`
- subscriber calls `motor_set_effort(...)` on those alias devices

Compile-time guards:

- build asserts ensure both aliases exist

## App Initialization

In `app/src/main.c`:

1. starts sensor subscriber
2. starts motor UDP service

## Relationship To Backend Drivers

Backends implement Zephyr devices and expose motor API:

- `modules/motor_driver/drivers/motor/esc_driver.c`
- `modules/motor_driver/drivers/motor/h_bridge_driver.c`
- `modules/motor_driver/drivers/motor/sim_motor_driver.c`

The app-level motor paths never call backend internals directly; they only use:

- `motor_set_effort(const struct device *dev, int32_t effort_q31)`

## Typical Developer Workflow

1. Build simulation firmware: `./scripts/build.zsh --sim`.
2. Required simulation runtime test: run `build/native_sim/zephyr/zephyr.exe` and validate expected shell/log behavior.
3. Build firmware for ESP32 validation target: `./scripts/build.zsh --no-flash`.
4. Run `motor list` and confirm expected devices.
5. Run `motor set <device_name> <percent>` for manual control.
6. Verify UDP sender path updates left/right through subscriber.
