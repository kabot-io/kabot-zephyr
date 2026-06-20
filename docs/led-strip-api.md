# LED Strip API

## Overview

The LED strip subsystem provides shell-driven control of a Zephyr LED strip device
defined in devicetree.

- Module name: `led_strip`
- Module root: [modules/led_strip](../modules/led_strip)
- Shell source: [modules/led_strip/subsys/led_strip/led_strip_shell.c](../modules/led_strip/subsys/led_strip/led_strip_shell.c)
- Board integration (current):
  - [app/boards/esp32s3_devkitc_esp32s3_procpu.conf](../app/boards/esp32s3_devkitc_esp32s3_procpu.conf)
  - [app/boards/esp32s3_devkitc_esp32s3_procpu.overlay](../app/boards/esp32s3_devkitc_esp32s3_procpu.overlay)

Unlike the motor module, this module currently exposes its public contract through
shell commands and devicetree wiring, while using Zephyr's built-in LED strip
driver API for hardware updates.

## Public Contract

The module provides a shell command group:

- `led_strip set <R> <G> <B> [N]`
- `led_strip strobe <pulse_ms> <total_ms> <count> [<R> <G> <B>] [<channel>]`

Implementation location:

- [modules/led_strip/subsys/led_strip/led_strip_shell.c](../modules/led_strip/subsys/led_strip/led_strip_shell.c)

Runtime behavior:

- validates the LED strip device is ready before any command executes
- validates all numeric inputs and returns `-EINVAL` for invalid values
- returns backend update errors directly from `led_strip_update_rgb(...)`
- maintains a persistent in-memory pixel buffer so partial updates preserve prior
  LED state

State scope:

- pixel state is persistent across shell commands during runtime
- pixel state is not persisted across reboot/reset

The shell command group is enabled by:

- `CONFIG_KABOT_LED_STRIP_SHELL=y`

defined in:

- [modules/led_strip/Kconfig](../modules/led_strip/Kconfig)

## Devicetree Integration

Current board-level wiring uses a named LED strip node and alias/chosen links.

Overlay location:

- [app/boards/esp32s3_devkitc_esp32s3_procpu.overlay](../app/boards/esp32s3_devkitc_esp32s3_procpu.overlay)

Key properties in current overlay:

- chosen entry:
  - `zephyr,led-strip = &led_strip;`
- alias entry:
  - `led-strip = &led_strip;`
- ws2812 over i2s node:
  - `compatible = "worldsemi,ws2812-i2s"`
  - `chain-length = <12>`
  - `color-mapping = <LED_COLOR_ID_GREEN LED_COLOR_ID_RED LED_COLOR_ID_BLUE>`
  - `reset-delay = <500>`

Shell compile-time assumptions:

- shell code resolves `DT_NODELABEL(led_strip)`
- build assert requires that node to exist
- pixel buffer size is derived from `chain-length`

This means command index bounds are tied directly to devicetree chain length.

Color mapping behavior:

- shell command inputs are logical RGB component values
- physical per-pixel byte order is determined by devicetree `color-mapping`

## Build Wiring

App-level module enablement:

- [app/CMakeLists.txt](../app/CMakeLists.txt) appends `../modules/led_strip` to `ZEPHYR_EXTRA_MODULES`

Module registration:

- [modules/led_strip/zephyr/module.yml](../modules/led_strip/zephyr/module.yml)
  - `cmake: .`
  - `kconfig: Kconfig`
  - `dts_root: .`

Module sources:

- [modules/led_strip/CMakeLists.txt](../modules/led_strip/CMakeLists.txt)
  - builds `subsys/led_strip/led_strip_shell.c` when
    `CONFIG_KABOT_LED_STRIP_SHELL` is enabled

Board Kconfig enablement (current ESP32-S3 target):

- [app/boards/esp32s3_devkitc_esp32s3_procpu.conf](../app/boards/esp32s3_devkitc_esp32s3_procpu.conf)
  - `CONFIG_LED_STRIP=y`
  - `CONFIG_WS2812_STRIP_I2S=y`
  - `CONFIG_I2S=y`
  - `CONFIG_I2S_ESP32=y`
  - `CONFIG_DMA=y`

Support scope note:

- current integration and validation scope is `esp32s3_devkitc_esp32s3_procpu`
- other boards require equivalent devicetree node labeling and Kconfig enablement

## Backend Assumptions

The module depends on Zephyr LED strip driver interfaces rather than defining a
custom device API class.

- Zephyr API call used for output updates:
  - `led_strip_update_rgb(const struct device *dev, struct led_rgb *pixels, size_t num_pixels)`
- Current hardware path is ws2812 over i2s, configured in board devicetree

As implemented, this module is a shell-oriented integration layer around Zephyr's
LED strip subsystem and board-specific devicetree configuration.

Concurrency assumption:

- shell logic uses one shared mutable pixel buffer with no internal locking
- command execution is expected to be serialized in normal shell usage

## See Also

- [led-strip-app-usage.md](led-strip-app-usage.md)
- [quality-gates-checklist.md](quality-gates-checklist.md)
- [README.md](README.md)
- [README.md](../README.md)
