# Firmware Data Flow

This document describes the current firmware data path after migration to the `Control` protobuf message.

Implementation tutorial:

- For a step-by-step guide to adding real sensor state fragment publishers (and extending `State` end-to-end), see `docs/real-sensor-publisher-tutorial.md`.

## Overview

The firmware uses UDP ingress and zbus-based loose coupling.

The architecture follows a single machine-mirror duplex backbone:

- Control ingress backbone: UDP -> `control_service` -> `control_channel` -> control-focused subscribers.
- State egress backbone: fragment publishers -> `state_channel` -> aggregate machine mirror -> periodic egress.
- Fragment workers (publishers/subscribers) own only their small `Control`/`State` portions and stay decoupled by channels.

- Network ingress: UDP socket receives binary protobuf datagrams.
- Decode: payload is decoded as `Control` (nanopb).
- Internal transport: decoded data is validated and published through zbus.
- Actuation: subscriber reads zbus message and applies motor effort.

## Ingress Pipeline (Control)

1. UDP packet arrives on configured control port (`30010`).
2. `control_service` decodes datagram as `Control`.
3. `control_channel_validator` validates normalized effort bounds:
   - `control.effort.state.x`
   - `control.effort.state.y`
4. Valid message is published on `control_channel` zbus channel.
5. `effort_subscriber` reads channel message and calls motor driver:
   - left motor effort <- `control.effort.state.x`
   - right motor effort <- `control.effort.state.y`

## Channel Architecture

Current internal channels:

- `control_channel`
  - Carries decoded and validated `Control` messages from UDP ingress.
  - Main consumers: motor actuation path and effort state publisher.
- `state_channel`
  - Carries partial `State` updates from producers.
  - Current producers: effort state publisher plus simulated and/or real IMU,
    magnetometer, distance, and dual-light publishers (selected by Kconfig).
- `state_egress_channel`
  - Carries periodically published merged `State` snapshots.
  - Consumed by UDP egress transport sender.

## State Update Policy

Target policy (agreed architecture):

- Each producer publishes partial state updates with producer-owned timestamps.
- State merge updates only fields whose incoming timestamp is newer than or equal to the stored timestamp.
- A periodic publisher emits combined state to egress transport.

Current implementation in this phase:

- `effort_state_publisher` subscribes to `control_channel`.
- On each control message, it captures effort values and stamps them with firmware uptime,
  and publishes a partial `State` update to `state_channel`.
- `sim_imu_publisher` runs periodically and publishes both
  `linear_acceleration` and `angular_velocity` with the same stamp and frame.
- `sim_magnetometer_publisher` runs periodically and publishes `magnetic_field`
  when simulated magnetometer mode is enabled.
- `magnetometer_publisher` runs periodically and publishes `magnetic_field`
  when the real MMC56X3 publisher is enabled.
- `sim_distance_publisher` runs periodically and publishes `distance`
  when simulated distance mode is enabled.
- `distance_publisher` runs periodically and publishes `distance`
  when the real VL53L0X publisher is enabled.
- `light_publisher` runs periodically and publishes valid
  `light_left` and/or `light_right` fields when the real dual LTR329 publisher is enabled.
  Invalid conversion samples are skipped (no fallback spike values are published).
- `state_aggregator_listener` is notified synchronously on `state_channel` publishes and
  merges incoming partial updates into the cached aggregate using update-if-newer-or-equal by field.
- `state_periodic_publisher` periodically requests a full snapshot from the aggregator cache
  and publishes it to `state_egress_channel`.
- `state_udp_sender` encodes snapshots from `state_egress_channel` and sends UDP.

Merge comparator details:

- For each optional field (`effort`, `linear_acceleration`, `angular_velocity`, `magnetic_field`, `distance`, `light_left`, `light_right`),
  replacement requires an incoming field header with a timestamp.
- If the cached field is missing, incoming replaces it.
- If cached and incoming fields are both stamped, incoming replaces cached when
  `incoming_stamp >= cached_stamp`.
- This tie-accepting behavior intentionally allows last-writer-wins on equal timestamps.

## Frame IDs and Refresh Rates

Default frame IDs:

- top-level `State.header.frame_id`: `base_link`
- `State.effort.header.frame_id`: `motors`
- `State.linear_acceleration.header.frame_id`: `imu`
- `State.angular_velocity.header.frame_id`: `imu`
- `State.magnetic_field.header.frame_id`: `mag`
- `State.distance.header.frame_id`: `tof`
- `State.light_left.header.frame_id`: `light_left`
- `State.light_right.header.frame_id`: `light_right`

Default publisher refresh rates (current config defaults):

- IMU publisher (`linear_acceleration` + `angular_velocity`): 10 Hz (`100 ms`)
- Magnetometer publisher (`magnetic_field`): 5 Hz (`200 ms`)
- Distance publisher (`distance`): approximately 60 Hz (`17 ms`)
- Light publisher (`light_left` + `light_right`): 2 Hz (`500 ms`)

## Ports

- Control ingress UDP port: `30010`.
- State egress UDP port: `30011`.
- HMI state listen UDP port: `30011`.

## Key Modules

- `app/src/control/control_service.c`
  - UDP socket setup, receive, decode, and publish.
- `app/src/zbus/channels/control_channel.c`
  - zbus channel definition and message validator.
- `app/src/zbus/control/effort_subscriber.c`
  - message consume and motor driver actuation.
- `app/protos/state_control_msg.proto`
  - protobuf schema for both `Control` and `State`.

## Why zbus Here

- Keeps transport handling independent from actuation logic.
- Makes future transport swap (UDP -> zenoh) easier.
- Supports fan-out patterns and future state pipeline composition.

## Current State Path Status

- State schema is defined in `state_control_msg.proto`.
- Periodic state egress path is implemented in this phase.
- Additional producers (IMU/magnetometer/distance/light, simulated and/or real by config)
  feed partial updates into `state_channel`.

## Build Notes

- Nanopb source generation currently uses `app/protos/state_control_msg.proto`.
- Legacy `effort_msg.proto` has been removed from source.

## Expected Host Compatibility

Host HMI should serialize `Control` as:

- `control.effort.state.x` = left effort
- `control.effort.state.y` = right effort

This matches the firmware decode and actuation path described above.
