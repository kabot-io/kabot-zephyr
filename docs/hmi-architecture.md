# HMI Architecture

This document describes the greenfield host HMI located in `scripts/kabot_io`.

## Goals

- Provide a clear and maintainable host-side control tool.
- Preserve an MVC architecture for future growth into a full HMI.
- Keep transport payloads aligned with firmware protobuf definitions.
- Support keyboard-driven control with periodic send cadence.

## Status In Project Scope

The HMI in `scripts/kabot_io` is a local debug/reference tool.

Normative discovery and binding contracts for external HMIs are documented in:

- `docs/robot-discovery-and-binding-spec.md`
- `docs/hmi_centric_info/04_firmware_interface_for_hmi_developers.md`
- `docs/hmi_centric_info/05_system_data_flow_reference.md`

## Scope (Current)

- Sends `Control` protobuf messages over UDP to firmware ingress.
- Listens for `State` protobuf messages over UDP from firmware.
- Uses arrow keys to update target effort state.
- Supports both one-shot send and periodic send loop.
- Displays decoded State values in read-only fields.
- Displays per-header receive rate (`Hz`) columns derived in the controller.
- Provides rolling telemetry plots with an operator toggle to disable plotting when
  throughput exceeds host rendering capacity.
- Performs Bonjour discovery with unicast sweep based on host interface subnets.
- Scans smaller subnets first and larger subnets last.
- Includes localhost (`127.0.0.1`) probe path for simulation by default.
- Shows a discovered-robots list with metadata.
- Exposes `Scan`, `Claim Selected`, and `Unclaim` controls for explicit ownership control.

## Ports

- Control target port (firmware ingress): `30010`.
- Discovery target port (firmware discovery ingress): `30012`.
- State listen port (HMI ingress): `30011`.
- Default HMI state bind address: `0.0.0.0`.

## Discovery Runtime Notes

- Discovery emits runtime logs during scan progress.
- Interface subnets are scanned in ascending host-count order.
- Large subnets (for example Docker/ZeroTier `/16`) are scanned after smaller ones.
- Discovery requests are sent with `claim=false`.
- Operator-driven `Claim Selected` sends `claim=true` only to the selected robot.
- Operator-driven `Unclaim` sends `release=true` to currently claimed robot.
- Discovery timeout default is `3.0s` (with effective minimum response window handling).
- When switching robots, release of the previous robot is best-effort; claim of the
  newly selected robot still proceeds if release response is not received.
- If Linux `ip` command is unavailable or times out, subnet enumeration is skipped
  and localhost probe path still supports simulation workflows.
- Discovery applies an effective minimum response window and an extra localhost
  grace receive period to reduce missed localhost responses.

## Multi-Robot Selection Flow

1. Operator runs `Scan` to populate discovered robots list.
2. HMI receives multiple `BonjourResponse` packets and deduplicates entries.
3. Operator selects one robot row.
4. Operator runs `Claim Selected`.
5. HMI updates active control target to the claimed robot endpoint.

Unclaim flow:

1. Operator runs `Unclaim`.
2. HMI sends `Bonjour.release=true` to currently claimed robot.
3. On success, HMI clears active robot indicator and updates list row claim fields.

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
- Displays rolling plots for selected telemetry channels.
- Exposes an `Enable plots` checkbox to pause plot sampling/rendering load.
- Emits UI events (send, periodic toggle, key press/release, close).
- Does not implement transport or control logic.

### Controller

- Connects View callbacks to Model operations.
- Tracks currently pressed arrow keys.
- Maps active key set to left/right effort values.
- Runs periodic send loop and state transitions.
- Handles plot-toggle events and clears plot buffers when plotting is disabled.

## Plot Runtime Behavior

- Plot ingestion is best-effort and decoupled from state field updates.
- State fields continue updating even when plots are disabled.
- When `Enable plots` is unchecked:
  - new plot samples are skipped
  - pending redraw timers are cancelled
  - plot history buffers are cleared
  - canvases are redrawn empty
- This allows operators to preserve responsiveness when high-rate telemetry is present.

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
