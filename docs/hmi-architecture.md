# HMI Architecture

This document describes the greenfield host HMI located in `scripts/kabot_io`.

## Goals

- Provide a clear and maintainable host-side control tool.
- Preserve an MVC architecture for future growth into a full HMI.
- Keep transport payloads aligned with firmware protobuf definitions.
- Support keyboard-driven control with periodic send cadence.

## Scope (Current)

- Sends `Control` protobuf messages over UDP to firmware ingress.
- Listens for `State` protobuf messages over UDP from firmware.
- Uses arrow keys to update target effort state.
- Supports both one-shot send and periodic send loop.
- Displays decoded State values in read-only fields.
- Displays per-header receive rate (`Hz`) columns derived in the controller.

## Ports

- Control target port (firmware ingress): `30010`.
- State listen port (HMI ingress): `30011`.
- Default HMI state bind address: `0.0.0.0`.

## Channel and Policy Alignment

- Firmware `control_channel` -> effort state publisher -> `state_channel` -> UDP egress.
- Firmware now performs periodic merged state egress before UDP transport.
- HMI decodes `State` datagrams and refreshes `StateSnapshot` values.
- Firmware merge policy is implemented per field as update-if-newer-or-equal by timestamp.

## Hz Display Policy

The HMI `Hz` values are UI-derived from `State.*.header.stamp` deltas:

- `Hz` is computed only when a field stamp changes and `delta_ms > 0`.
- Instantaneous `Hz` values are smoothed using a moving average window of 5 samples.
- A per-field clear timer is started/refreshed after each successful `Hz` update.
- If no newer sample arrives before timeout (`2000 ms`), that field's `Hz` is cleared.
- Timer-based clear is wall-clock based, so stale `Hz` values are removed even when no new packets arrive
  (for example if firmware stops).
- If a stamp is invalid or non-monotonic for a field, that field's `Hz` is cleared immediately.

## Package Layout

- `scripts/kabot_io/main.py`: app entrypoint and startup wiring.
- `scripts/kabot_io/config.py`: runtime configuration defaults.
- `scripts/kabot_io/model.py`: networking and data model operations.
- `scripts/kabot_io/view.py`: Tkinter UI and field bindings.
- `scripts/kabot_io/controller.py`: input handling and control loop orchestration.
- `scripts/kabot_io/proto_codec.py`: protobuf compile/load/encode helpers.

## MVC Responsibilities

### Model

- Owns UDP socket lifecycle.
- Encodes `Control` payloads via generated protobuf classes.
- Sends control packets and stop sequence.
- Defines `StateSnapshot` shape used by the view.

### View

- Owns Tkinter widgets, variables, and callbacks.
- Displays control fields and read-only State fields.
- Emits UI events (send, periodic toggle, key press/release, close).
- Does not implement transport or control logic.

### Controller

- Connects View callbacks to Model operations.
- Tracks currently pressed arrow keys.
- Maps active key set to left/right effort values.
- Runs periodic send loop and state transitions.

## Control Behavior

- Arrow keys update desired effort state.
- `Send once` immediately transmits current effort values.
- Periodic mode transmits at the configured interval until stopped.
- If periodic mode is off, key changes update targets but do not transmit automatically.
- `q` closes the app via the registered stop callback.

## Protobuf Strategy

- On startup, `proto_codec.initialize_proto_runtime()` compiles:
  - `app/protos/state_control_msg.proto`
- Generated Python module is loaded from:
  - `scripts/kabot_io/_generated/state_control_msg_pb2.py`
- Control serialization uses generated class APIs, not handcrafted wire bytes.

## Rationale

- MVC keeps UI, control logic, and transport concerns separate.
- Runtime proto compile avoids schema drift between host and firmware.
- Centralized controller behavior provides predictable keyboard semantics.

## Next Steps

- Add CLI modes for send-only, receive-only, and combined operation.
- Add protocol/version checks for safer host-firmware compatibility.
