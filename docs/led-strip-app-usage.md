# LED Strip Usage In The App

## Overview

LED strip control in the app is currently provided through the module shell
commands and board devicetree wiring.

Primary implementation paths:

- shell command implementation:
  - [modules/led_strip/subsys/led_strip/led_strip_shell.c](../modules/led_strip/subsys/led_strip/led_strip_shell.c)
- module configuration:
  - [modules/led_strip/Kconfig](../modules/led_strip/Kconfig)
  - [modules/led_strip/CMakeLists.txt](../modules/led_strip/CMakeLists.txt)
- app build/module wiring:
  - [app/CMakeLists.txt](../app/CMakeLists.txt)
- current board wiring:
  - [app/boards/esp32s3_devkitc_esp32s3_procpu.conf](../app/boards/esp32s3_devkitc_esp32s3_procpu.conf)
  - [app/boards/esp32s3_devkitc_esp32s3_procpu.overlay](../app/boards/esp32s3_devkitc_esp32s3_procpu.overlay)

## LED Strip Shell Commands

Command group:

- `led_strip`

Subcommands:

- `led_strip set <R> <G> <B> [N]`
- `led_strip strobe <pulse_ms> <total_ms> <count> [<R> <G> <B>] [<channel>]`

Supported strobe invocation forms are exactly:

- `led_strip strobe <pulse_ms> <total_ms> <count>`
- `led_strip strobe <pulse_ms> <total_ms> <count> <channel>`
- `led_strip strobe <pulse_ms> <total_ms> <count> <R> <G> <B>`
- `led_strip strobe <pulse_ms> <total_ms> <count> <R> <G> <B> <channel>`

### Set Command

Usage:

- `led_strip set <R> <G> <B> [N]`

Arguments:

- `<R>`, `<G>`, `<B>`: color components in range `[0, 255]`
- `[N]`: optional 0-based LED index in range `[0, chain_length - 1]`

Behavior:

- if `N` is omitted, all LEDs are set to the same color
- if `N` is provided, only that LED is updated
- updates are applied using `led_strip_update_rgb(...)` with the full persistent
  pixel buffer

Persistence scope:

- the pixel buffer persists across shell commands during runtime
- the buffer is RAM-only state and is not persisted across reboot/reset

Examples:

- set all LEDs to red:
  - `led_strip set 255 0 0`
- set LED 3 to blue:
  - `led_strip set 0 0 255 3`

### Strobe Command

Usage:

- `led_strip strobe <pulse_ms> <total_ms> <count> [<R> <G> <B>] [<channel>]`

Required arguments:

- `<pulse_ms>`: pulse ON duration, range `[1, 10000]`
- `<total_ms>`: total burst duration, range `[1, 600000]`
- `<count>`: number of pulses, range `[1, 10000]`

Optional arguments:

- optional color `<R> <G> <B>` with each component in `[0, 255]`
- optional `<channel>` in `[0, chain_length - 1]`

Defaults and selection rules:

- default strobe color is `(10, 10, 10)`
- if channel is omitted, a random LED is chosen for each pulse
- if channel is provided, all pulses use that fixed LED
- if color is omitted, default color is used

Validation rule:

- `pulse_ms * count` must be less than or equal to `total_ms`

Behavior:

- each pulse overlays the strobe color on one LED
- after `pulse_ms`, the previous color for that LED is restored
- the command preserves and restores prior LED state via persistent pixel buffer
- the command runs synchronously and blocks the shell command context until all
  pulses complete

Examples:

- default-color random-channel strobe:
  - `led_strip strobe 50 2000 10`
- green strobe on random channel each pulse:
  - `led_strip strobe 80 3000 12 0 255 0`
- blue strobe on fixed channel 4:
  - `led_strip strobe 100 5000 20 0 0 255 4`

## Device Readiness and Errors

Both commands:

- fail with `-ENODEV` if the LED strip device is not ready
- fail with `-EINVAL` for invalid argument format/range
- propagate negative errors returned by `led_strip_update_rgb(...)`

This gives direct feedback when devicetree wiring, bus support, or runtime
driver setup is incorrect.

## Board-Specific Runtime Wiring

Current ESP32-S3 board integration configures ws2812 over i2s.

Current support scope:

- this documentation reflects and validates behavior on
  `esp32s3_devkitc_esp32s3_procpu`
- other boards may work only if they provide equivalent Kconfig and devicetree
  wiring expected by this module

- Kconfig enablement in:
  - [app/boards/esp32s3_devkitc_esp32s3_procpu.conf](../app/boards/esp32s3_devkitc_esp32s3_procpu.conf)
- devicetree node and alias/chosen links in:
  - [app/boards/esp32s3_devkitc_esp32s3_procpu.overlay](../app/boards/esp32s3_devkitc_esp32s3_procpu.overlay)

The shell implementation is tied to the node label `led_strip`, and command index
bounds derive from `chain-length` in that node.

Color mapping note:

- shell inputs are logical `<R> <G> <B>` values
- physical byte order on the strip follows devicetree `color-mapping`

Concurrency note:

- shell code uses one shared pixel buffer without internal locking
- assume serialized command usage from one shell context unless explicit
  synchronization is added in future updates

## Typical Developer Workflow

1. Build firmware for ESP32 validation target: `./scripts/build.zsh --no-flash`.
2. Flash and connect shell.
3. Verify command availability with shell help for `led_strip`.
4. Run `led_strip set` to confirm static color updates.
5. Run `led_strip strobe` to validate pulse timing, random/fixed channel behavior,
   and color overlay/restore behavior.
