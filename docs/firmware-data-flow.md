# Firmware Data Flow

This document describes the current firmware data path after migration to the `Control` protobuf message.

## Overview

The firmware uses UDP ingress and zbus-based loose coupling.

- Network ingress: UDP socket receives binary protobuf datagrams.
- Decode: payload is decoded as `Control` (nanopb).
- Internal transport: decoded data is validated and published through zbus.
- Actuation: subscriber reads zbus message and applies motor effort.

## Ingress Pipeline (Control)

1. UDP packet arrives on configured control port (`30010`).
2. `motor_service` decodes datagram as `Control`.
3. `control_channel_validator` validates normalized effort bounds:
   - `control.effort.state.x`
   - `control.effort.state.y`
4. Valid message is published on `control_channel` zbus channel.
5. `control_subscriber` reads channel message and calls motor driver:
   - left motor effort <- `control.effort.state.x`
   - right motor effort <- `control.effort.state.y`

## Channel Architecture

Current internal channels:

- `control_channel`
  - Carries decoded and validated `Control` messages from UDP ingress.
  - Main consumers: motor actuation path and state effort listener.
- `state_channel`
  - Carries partial `State` updates from producers.
  - Current producer: effort state listener.
- `state_egress_channel`
  - Carries periodically published merged `State` snapshots.
  - Consumed by UDP egress transport sender.

## State Update Policy

Target policy (agreed architecture):

- Each producer publishes partial state updates with producer-owned timestamps.
- State merge updates only fields whose incoming timestamp is newer than the stored timestamp.
- A periodic publisher emits combined state to egress transport.

Current implementation in this phase:

- `effort_state_publisher` subscribes to `control_channel`.
- On each control message, it captures effort values, stamps them with `k_uptime_get()`,
  and publishes a partial `State` update to `state_channel`.
- `state_periodic_publisher` merges incoming updates with update-if-newer timestamp policy
  and publishes merged snapshots periodically to `state_egress_channel`.
- `state_udp_sender` encodes snapshots from `state_egress_channel` and sends UDP.

## Ports

- Control ingress UDP port: `30010`.
- State egress UDP port: `30011`.
- HMI state listen UDP port: `30011`.

## Key Modules

- `app/src/motor/motor_service.c`
  - UDP socket setup, receive, decode, and publish.
- `app/src/zbus/control_channel.c`
  - zbus channel definition and message validator.
- `app/src/zbus/control_subscriber.c`
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
- Additional producers (IMU/encoder/distance) can now feed partial updates into `state_channel`.

## Build Notes

- Nanopb source generation currently uses `app/protos/state_control_msg.proto`.
- Legacy `effort_msg.proto` has been removed from source.

## Expected Host Compatibility

Host HMI should serialize `Control` as:

- `control.effort.state.x` = left effort
- `control.effort.state.y` = right effort

This matches the firmware decode and actuation path described above.
